#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>

#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../pool/sqlconnRAII.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../http/httpconn.h"
#include "epoller.h"




class WebServer{

public:
    WebServer(int port, int trigMode, int timeoutMs, bool OptLinger,
    int sqlPort, const char* sqlUser, const char* sqlPwd, const char* dbName,
    int connPoolNum, int threadNum, 
    bool openLog, int LogLevel, int logQueSize);

    ~WebServer();
    void Start();
    
private:
    void InitEventMode_(int trigMode);
    bool InitSocket_();

    void DealListen_();
    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);

    void CloseConn_(HttpConn* client);
    void AddClient_(int fd, sockaddr_in addr);
    void ExtentTime_(HttpConn* client);
    void SendError_(int fd, const char* info); 


    void OnWrite_(HttpConn* client);
    void OnProcess(HttpConn* client);
    void OnRead_(HttpConn* client);

    int SetFdNonBlock(int fd);

    static const int MAX_FD = 65536;

    int port_;
    int timeoutMS_;
    bool isClose_;
    int listenFd_;
    bool openLinger_;
    char* srcDir_;

    uint32_t listenEvent_;
    uint32_t connEvent_;

    std::unique_ptr<HeapTimer> timer_;
    std::unique_ptr<ThreadPool> threadpool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int,HttpConn> users_;//<fd,HttpConn>

};



#endif