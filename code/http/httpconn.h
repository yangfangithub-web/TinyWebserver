#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>

#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"

class HttpConn {
public:
    HttpConn();
    ~HttpConn();
    void init(int fd, const sockaddr_in& addr);

    bool process();

    ssize_t read(int* saveErrno);
    ssize_t write(int* saveErro);

    void Close();

    //相关方法
    int ToWriteBytes();
    bool IsKeepAlive() const;
    int GetFd() const;
    int GetPort() const;
    const char* GetIP() const;
    sockaddr_in GetAddr() const;

    static bool isET;
    static const char* srcDir;
    static std::atomic<int> userCount;

    

private:
    int fd_;
    struct sockaddr_in addr_;

    bool isClose_;

    struct iovec iov_[2];
    int iovCnt_;//表示缓冲区的个数

    Buffer readBuff_;
    Buffer writeBuff_;

    HttpRequest request_;
    HttpResponse response_;


};



#endif