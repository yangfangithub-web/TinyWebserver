#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H


#include <unordered_map>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string>
#include <string.h>


#include "../buffer/buffer.h"
#include "../log/log.h"
class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();
    void Init(const std::string& srcDir,std::string& path,bool isKeepAlive = false, int code=-1);
    
    void MakeResponse(Buffer& buff);
    void ErrorContent(Buffer& buff, std::string message);
    void UnmapFile();
    int Code() const{return code_;}
    char* File();
    size_t FileLen() const;

private:
    void AddStateLine_(Buffer &buff);
    void AddHeader_(Buffer &buff);
    void AddContent_(Buffer &buff);
    void ErrorHtml_();
    std::string GetFileType_();

    int code_;//状态码
    bool isKeepAlive_;

    std::string path_;
    std::string srcDir_;

    struct stat mmFileStat_;//保存获取到的文件的属性信息
    char* mmFile_;


    static const std::unordered_map<std::string,std::string> SUFFIX_TYPE;
    static const std::unordered_map<int,std::string> CODE_STATUS; //编码状态集
    static const std::unordered_map<int,std::string> CODE_PATH;   //编码路径集


};

#endif