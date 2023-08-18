#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <errno.h>
#include <string>
#include <mysql/mysql.h>
#include <algorithm>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnRAII.h"

class HttpRequest {
public:
    enum PARSE_STATE {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,
        };

    enum HTTP_CODE {
        NO_REQUEST=0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };

    HttpRequest(){ Init(); }
    ~HttpRequest() = default;

    void Init();

    bool Parse(Buffer& buff);
    bool IsKeepAlive() const;

    std::string& path();
    std::string path() const;//const函数不能修改其数据成员
    std::string method() const;
    std::string version() const;
    //POST
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;
    

private:

    bool ParseRequestLine_(const std::string &line);
    void ParseHeader_(const std::string &line);
    void ParseBody_(const std::string &line);
    void ParsePath_();

    //POST
    void ParsePost_();
    void ParseFromUrlencoded_();
    static bool UserVerify(const std::string& name, const std::string& pwd,bool isLogin);//用户登录验证
    static int ConverHex(char ch);//16进制转10进制


    PARSE_STATE state_;
    std::string method_;//方法GET/POST
    std::string path_;//url
    std::string version_;//版本
    std::string body_;//请求体
    std::unordered_map<std::string,std::string> header_;
    std::unordered_map<std::string,std::string> post_;//ParseFromUrlencoded_()解析的结果存在这里
    
    //POST

    static const std::unordered_map<std::string,int> DEFAULT_HTML_TAG;//登录、注册
    static const std::unordered_set<std::string> DEFAULT_HTML;//存储界面路径

};


#endif