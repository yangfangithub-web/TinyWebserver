#include <unistd.h>
#include "server/webserver.h"

int main() {

    WebServer server(
        1316,3,60000,false,//port , ET模式，timeoutMs ,优雅退出
        3306, "debian-sys-maint","JpR681ifBXj9lTq7","mydb",//mysql配置
        12,8,true,1,0
    );
    server.Start();
}