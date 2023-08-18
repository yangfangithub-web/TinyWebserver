#include "httpconn.h"
using namespace std;


bool HttpConn::isET;
const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;

HttpConn::HttpConn() {
    fd_ = -1;
    addr_ = {0};
    isClose_ = true;
}

HttpConn::~HttpConn(){
    Close();
}

void HttpConn::init(int fd, const sockaddr_in& addr){
    assert(fd>0);
    userCount++;
    addr_ = addr;
    fd_ = fd;
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d",fd_,GetIP(),GetPort(),(int)userCount);
}

void HttpConn::Close() {
    response_.UnmapFile();
    if(isClose_ == false) {
        isClose_ = true;
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d)quit, UserCount:%d",fd_,GetIP(),GetPort(),(int)userCount);
    }
}

bool HttpConn::process() {
    
    request_.Init();
    
    if(readBuff_.ReadableBytes()<=0) {
        return false;
    }
    else if(request_.Parse(readBuff_)) {
        LOG_DEBUG("%s",request_.path().c_str());
        
        response_.Init(HttpConn::srcDir,request_.path(),request_.IsKeepAlive(),200);
        
    } else {
        response_.Init(HttpConn::srcDir,request_.path(),false,400);
    }

    response_.MakeResponse(writeBuff_);

    //响应头
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peak());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;
    //文件
    if (response_.FileLen() > 0 && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    LOG_DEBUG("filesize:%d, %d to %d", response_.FileLen(),iovCnt_,ToWriteBytes());
    return true;
}

//将fd_里内容读到readBuff_中，然后HTTPRequest解析报文
ssize_t HttpConn::read(int* saveErrno){
    //printf("httpconn read IN\n");
    ssize_t len = -1;
    do{
        len = readBuff_.ReadFd(fd_,saveErrno);
        if (len <= 0) {
            break;
        }
    }while(isET);
    //printf("已将fd_内容读到readBuff中");
    return len;
}

//将response生成在writeBuff中的响应报文写到fd_中
ssize_t HttpConn::write(int* saveErrno){
    ssize_t len = -1;
    do {
        len = writev(fd_, iov_, iovCnt_);
        if(len <=0 )  {
            *saveErrno = errno;
            break;
        }
        if (iov_[0].iov_len + iov_[1].iov_len == 0) {break;}
        else if (static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = (uint8_t*)iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if (iov_[0].iov_len) {
                writeBuff_.RetrieveAll();
                iov_[0].iov_len=0;
            }
        }
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len;
            iov_[0].iov_len -= len;
            writeBuff_.Retrieve(len);
        }
    } while (isET || ToWriteBytes() > 10240);
    return len;
}



//相关方法
int HttpConn::ToWriteBytes(){ 
    return iov_[0].iov_len + iov_[1].iov_len; 
}

bool HttpConn::IsKeepAlive() const{
    return request_.IsKeepAlive();
}

int HttpConn::GetFd() const{
    return fd_;
}
int HttpConn::GetPort() const{
    return addr_.sin_port;
}
const char* HttpConn::GetIP() const{
    return inet_ntoa(addr_.sin_addr);
}
struct sockaddr_in HttpConn::GetAddr() const{
    return addr_;
}
