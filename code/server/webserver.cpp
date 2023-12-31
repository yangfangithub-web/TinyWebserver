#include "webserver.h"
using namespace std;


WebServer::WebServer(int port, int trigMode, int timeoutMs, bool OptLinger,
    int sqlPort, const char* sqlUser, const char* sqlPwd, const char* dbName,
    int connPoolNum, int threadNum, bool openLog, int logLevel, int logQueSize):
    port_(port),timeoutMS_(timeoutMs),openLinger_(OptLinger),isClose_(false),
    timer_(new HeapTimer()),threadpool_(new ThreadPool(threadNum)),epoller_(new Epoller())
    {
        if(openLog == true) {
            Log::Instance()->init(logLevel,"./log",".log",logQueSize);
        }
        
        //初始化HTTP连接
        srcDir_ = getcwd(nullptr, 256);
        assert(srcDir_);
        strncat(srcDir_,"/resources2/",16);
        HttpConn::userCount = 0; //初始化客户连接数量
        HttpConn::srcDir = srcDir_; //初始化HTTP资源路径
        
        //初始化Sql连接
        SqlConnPool::Instance()->Init("localhost",sqlPort,sqlUser,sqlPwd,dbName,connPoolNum);
        
        //设置IO epoll检测模式
        InitEventMode_(trigMode);//

        //初始化socket连接
        if(!InitSocket_()) {isClose_ = true;}

        //向日志中写入记录
        if (isClose_) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port: %d, OpenLinger: %s", port_, OptLinger?"true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",(listenEvent_ & EPOLLET ? "ET":"LT"),(connEvent_ & EPOLLET ? "ET":"LT"));
            LOG_INFO("LogLevel: %d", logLevel);
            LOG_INFO("srcDir: %s",HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
        
    }

WebServer::~WebServer(){
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

void WebServer::Start(){
    int timeMS = -1;
    if(!isClose_) {LOG_INFO("=================Server start!==================");}
    while(!isClose_) {
        //通过timer更新timeMS
        if (timeoutMS_ > 0) {
            timeMS = timer_->GetNextTick();
        }
        int eventCnt = epoller_->Wait(timeMS);
        for (int i = 0; i<eventCnt; i++) {
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            if (fd == listenFd_) {
                
                DealListen_();
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                
                CloseConn_(&users_[fd]);
            }
            else if(events & EPOLLIN) {
                assert(users_.count(fd)>0);
                
                DealRead_(&users_[fd]);
                
            }
            else if(events & EPOLLOUT) {
                assert(users_.count(fd)>0);
               
                DealWrite_(&users_[fd]);
            }
            else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void WebServer::DealListen_(){
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_,(struct sockaddr*)&addr,&len);
        if(fd<=0) return;
        else if(HttpConn::userCount >= MAX_FD) {
            SendError_(fd,"Server busy!");
            LOG_WARN("Client is full!");
            return;
        }
        AddClient_(fd,addr);
    }while(listenEvent_ && EPOLLET);
}

void WebServer::SendError_(int fd, const char* info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret<0) {
        LOG_WARN("send error to client [%d] error!",fd);
    }
    close(fd);
}

void WebServer::AddClient_(int fd, sockaddr_in addr) {
    
    assert(fd > 0);
    users_[fd].init(fd,addr);
    if(timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_,this,&users_[fd]));
    }
    epoller_->AddFd(fd,EPOLLIN | connEvent_);
    SetFdNonBlock(fd);
    LOG_INFO("Client[%d] in!",users_[fd].GetFd());
}

void WebServer::DealWrite_(HttpConn* client){
    assert(client);
    ExtentTime_(client);//延长时间
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    if(timeoutMS_>0) {
        timer_->adjust(client->GetFd(),timeoutMS_);
    }
}

void WebServer::DealRead_(HttpConn* client){
    //LOG_DEBUG("DealRead IN");
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
    
}


void WebServer::InitEventMode_(int trigMode){
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigMode) {
        case 0:
            break;
        case 1:
            connEvent_ |= EPOLLET;
            break;
        case 2:
            listenEvent_ |= EPOLLET;
            break;
        case 3:
            listenEvent_ |= EPOLLET;
            connEvent_ |= EPOLLET;
            break;
        default:
            listenEvent_ |= EPOLLET;
            connEvent_ |= EPOLLET;
            break;
    }
    HttpConn::isET =  (connEvent_ & EPOLLET);
}

bool WebServer::InitSocket_(){
    int ret;
    struct sockaddr_in addr;
    if (port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port: %d error !", port_);
        return false;
    } 

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    struct linger optLinger = {0};//用于设置套接字关闭方式
    if (openLinger_) {
        optLinger.l_onoff = 1;// 非零表示启用延迟关闭，零表示禁用
        optLinger.l_linger = 1;// 延迟关闭的时间，单位为秒
    } 

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        LOG_ERROR("Create socket error!",port_);
        return false;
    }  

    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    } 

    int optval = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR,(const void*)&optval, sizeof(int));
    if (ret == -1) {
        LOG_ERROR("set socket setsockopt error!");
        close(listenFd_);
        return false;
    } 

    ret = bind(listenFd_,(struct sockaddr*)&addr,sizeof(addr));
    if(ret<0) {
        LOG_ERROR("Bind port: %d error!", port_);
        close(listenFd_);
        return false;
    } 

    ret = listen(listenFd_,6);
    if(ret<0) {
        LOG_ERROR("Listen port: %d error!",port_);
        close(listenFd_);
        return false;
    }

    ret = epoller_->AddFd(listenFd_, listenEvent_| EPOLLIN);
    if (ret==0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    } 

    SetFdNonBlock(listenFd_);
    
    LOG_INFO("Server port: %d", port_);
    return true;
}

int WebServer::SetFdNonBlock(int fd) {
    assert(fd > 0);
    return fcntl(fd,F_SETFL,fcntl(fd,F_GETFD,0)|O_NONBLOCK);
}


void WebServer::CloseConn_(HttpConn* client){
    assert(client);
    LOG_INFO("Client[%d] quit!",client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

void WebServer::OnWrite_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if (client->ToWriteBytes() == 0) {
        if (client->IsKeepAlive()) {
            OnProcess(client);
            return;
        }
    }
    else if (ret < 0) {
        if (writeErrno == EAGAIN) {
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

void WebServer::OnRead_(HttpConn* client) {
    
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN) {
        CloseConn_(client);
        return;
    }
    OnProcess(client);//处理并重置文件描述符
}

void WebServer::OnProcess(HttpConn* client) {
    
    if(client->process()) {
        epoller_->ModFd(client->GetFd(), connEvent_| EPOLLOUT);
    }
    else {
        epoller_->ModFd(client->GetFd(),connEvent_|EPOLLIN);
    }
}