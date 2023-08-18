
#ifndef LOG_H
#define LOG_H

#include <string.h>
#include <mutex>
#include <sys/time.h>
#include <assert.h>
#include <sys/stat.h>
#include <thread>
#include <stdarg.h>
#include <stdio.h>

#include "blockqueue.h"
#include "../buffer/buffer.h"

//单例模式
class Log{
public:
    static Log* Instance();//单例模式，局部静态变量获取实例
    static void FlushLogThread();//异步写日志线程

    void init(int level, const char* path="./log",const char* suffix=".log",int maxQueueCapacity = 1024);

    void write(int level, const char *format,...);//将输出内容按照标准格式整理
    void flush();


    int GetLevel();
    void SetLevel(int level);
    bool IsOpen() {return isOpen_;}

private:
    Log();//构造函数私有化
    virtual ~Log();

    void AppendLogLevelTitle_(int level);

    void AsyncWrite_();//异步写日志的方法


private:
    //C++11中，常量，变量，静态常量 ----------可以在类内初始化
    //         静态变量----------------------不可以在类内初始化
    static const int LOG_NAME_LEN = 256;//日志文件最长文件名
    static const int LOG_PATH_LEN = 256;//日志文件最长路径名
    static const int MAX_LINES = 50000;//日志文件最长日志条数


    int toDay_;//当天日期
    int lineCount_;//日志行数记录
    int MAX_LINES_;//最大日志行数

    const char* path_;//路径名
    const char* suffix_;//后缀名

    int level_;//日志分级
    bool isAsync_;//是否为异步日志
    bool isOpen_;//日志文件是否打开

    FILE* fp_;//打开log文件的指针
    Buffer buff_;//写缓冲区

    std::unique_ptr<BlockDeque<std::string>> deque_;//异步阻塞队列，每一个消息是string类型的
    std::unique_ptr<std::thread> writeThread_;//写线程的指针
    std::mutex mtx_;
    
};


//外部使用日志统一的方法
#define LOG_BASE(level,format,...) \
do{\
    Log* log = Log::Instance();\
    if (log->IsOpen() && log->GetLevel()<=level) {\
        log->write(level,format,##__VA_ARGS__);\
        log->flush();\
    }\
    \
} while(0);

//外部使用日志的接口
#define LOG_DEBUG(format,...) do{LOG_BASE(0,format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format,...) do{LOG_BASE(1,format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format,...) do{LOG_BASE(2,format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format,...) do{LOG_BASE(3,format, ##__VA_ARGS__)} while(0);
#endif