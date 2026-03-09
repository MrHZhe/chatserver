#include "chatserver.hpp"
#include "chatservice.hpp"
#include <signal.h>
#include <iostream>
#include <string>
using namespace std;
//重置user信息
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}
int main(int argc, char **argv){
    // 判断命令行的参数个数是否符合要求
    if (argc < 3) {
        cerr << "command invalid! example: ./ChatServer 127.0.0.1 6000" << endl;
        exit(-1);
    }
    // 解析通过命令行参数传递的 ip 和 port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]); // 这里用 atoi 将字符串转成整型端口号
    signal(SIGINT,resetHandler);
    EventLoop loop;
    InetAddress addr(ip,port);
    ChatServer server(&loop,addr,"ChatServer");
    
    server.start();
    loop.loop();

    return 0;
}