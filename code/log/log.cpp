#include "log.h"

using namespace std;

Log::Log() {
    lineCount_=0;
    isAsync_=false;
    writeThread_ = nullptr;
    deque_=nullptr;
    toDay_=0;
    fp_=nullptr;
}
Log::~Log() {
    if(writeThread_ && writeThread_->joinable()) {
        while(!deque_->empty()) {
            deque_->flush();
        }
        deque_->Close();
        writeThread_->join();
    }
    if(fp_) {
        lock_guard<mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}

Log* Log::Instance() {
    static Log inst;
    return &inst;
}

void Log::init(int level=1, const char* path,const char* suffix,int maxQueueCapacity) {
    isOpen_=true;
    level_=level;
    if (maxQueueCapacity>0) {
        isAsync_=true;
        if(!deque_) {
            unique_ptr<BlockDeque<string>> newDeque(new BlockDeque<std::string>);
            deque_ = move(newDeque);

            std::unique_ptr<std::thread> NewThread(new thread(FlushLogThread));
            writeThread_= move(NewThread);
        }
    }else{
        isAsync_=false;
    }

    lineCount_ = 0;

    time_t timer = time(nullptr);
    struct tm *sysTime=localtime(&timer);
    struct tm t=*sysTime;
    path_=path;
    suffix_=suffix;
    char fileName[LOG_NAME_LEN]={0};
    snprintf(fileName,LOG_NAME_LEN-1,"%s/%04d_%02d_%02d%s",path_,t.tm_year+1900,t.tm_mon+1,t.tm_mday,suffix_);
    toDay_ = t.tm_mday;

    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll();//清空buff_缓冲区
        if(fp_) {
            flush();
            fclose(fp_);
        }
        fp_ = fopen(fileName,"a");
        if (fp_ == nullptr) {
            mkdir(path_,0777);
            fp_ = fopen(fileName,"a");
        }
        assert(fp_ != nullptr);
    }
}

void Log::write(int level, const char *format,...) {
    struct timeval now = {0,0};
    gettimeofday(&now,nullptr);
    time_t tSec = now.tv_sec;//获取当前时间
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;//获取系统时间
    va_list vaList;

    //创建新的log文件的条件
    //1. 当前天 ！= 创建日志的时间
    //或者
    //2. lineCount_不为0，且到了最大行数限制
    if (toDay_!=t.tm_mday || (lineCount_ && (lineCount_ % MAX_LINES == 0))) {
        unique_lock<mutex> locker(mtx_);
        locker.unlock();

        char newFile[LOG_NAME_LEN];
        char tail[36]={0};
        snprintf(tail,36,"%04d_%02d_%02d",t.tm_year+1900,t.tm_mon+1,t.tm_mday);

        if (toDay_ != t.tm_mday) {//当天的新日志
            snprintf(newFile,LOG_NAME_LEN-72,"%s/%s%s",path_,tail,suffix_);
            toDay_=t.tm_mday;
            lineCount_=0;
        }
        else {//当天增加的第lineCount_/MAX_LINES个日志
            snprintf(newFile,LOG_NAME_LEN-72,"%s/%s-%d%s",path_,tail,(lineCount_/MAX_LINES),suffix_);
        }
        locker.lock();
        flush();

        fclose(fp_);
        fp_ = fopen(newFile,"a");//打开文件附加写入	若不存在，则创建新文件写入
        assert(fp_!=nullptr);//assert其值为假则输出错误信息

    }

    //不管是否创建新文件都要执行这一部分
    //在buff_内生成一条对应的日志信息
    {
        //添加时间信息
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        int n = snprintf(buff_.BeginWrite(),128,"%04d-%02d-%02d %02d:%02d:%02d.%06ld",
            t.tm_year+1900,t.tm_mon+1,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec,now.tv_usec);
        buff_.HasWritten(n);
        //添加消息头
        AppendLogLevelTitle_(level);
        //添加其他信息
        va_start(vaList,format);
        int m=vsnprintf(buff_.BeginWrite(),buff_.WritableBytes(),format,vaList);
        va_end(vaList);
        buff_.HasWritten(m);
        //添加回车换行
        buff_.Append("\n\0",2);

        if(isAsync_ && deque_ && !deque_->full()) {//异步方式，放入阻塞队列
            deque_->push_back(buff_.RetrieveAllToStr());
        } else {//同步方式，直接写入
            fputs(buff_.Peak(),fp_);
        }
        buff_.RetrieveAll();
    }
}

void Log::AppendLogLevelTitle_(int level) {
    switch(level) {
        case 0:
            buff_.Append("[debug]: ",9);
            break;
        case 1:
            buff_.Append("[info] : ",9);
            break;
        case 2:
            buff_.Append("[warn] : ",9);
            break;
        case 3:
            buff_.Append("[error]: ",9);
            break;
        default:
            buff_.Append("[info] : ",9);
            break;
    }
}

//唤醒阻塞队列消费者，开始写日志
void Log::flush() {
    if (isAsync_) {
        deque_->flush();
    }
    fflush(fp_);//清空输入缓冲区
}

//异步写日志

void Log::FlushLogThread(){
    Log::Instance()->AsyncWrite_();
}

void Log::AsyncWrite_() {
    string str="";
    while(deque_->pop(str)) {
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(),fp_);
    }
}




int Log::GetLevel() {
    lock_guard<mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level) {
    lock_guard<mutex> locker(mtx_);
    level_=level;
}