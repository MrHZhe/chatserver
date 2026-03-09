#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;
// 显示当前登录成功用户的基本信息
void showCurrentUserData();
// bool全局变量，控制聊天页面程序
bool isMainMenuRunning = false;
// 接收线程
void readTaskHandler(int clientfd);
// 获取系统时间
string getCurrentTime();
// 主聊天页面程序
void mainMenu(int clientfd);

void help(int fd = 0,string str = "");
void chat(int,string);
void addfriend(int,string);
void creategroup(int,string);
void addgroup(int,string);
void groupchat(int,string);
void loginout(int,string);

//系统支持的客户端命令列表
unordered_map<string,string> commandMap ={
    {"help","显示所有支持的命令,格式help"},
    {"chat","一对一聊天,格式chat:friendid:message"},
    {"addfriend","添加好友,格式addfriend:friendid"},
    {"creategroup","创建群组,格式creategroup:groupname:groupdesc"},
    {"addgroup","加入群组,格式addgroup:groupid"},
    {"groupchat","群聊,格式groupchat:groupid:message"},
    {"loginout","注销,格式loginout"}
};

//注册系统支持的客户端命令处理
unordered_map<string,function<void(int,string)>> commandHandlerMap = {
    {"help",help},
    {"chat",chat},
    {"addfriend",addfriend},
    {"creategroup",creategroup},
    {"addgroup",addgroup},
    {"groupchat",groupchat},
    {"loginout",loginout}
};

// 聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "command invalid! example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }
    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        cerr << "socket creat error" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息ip+port
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client和server进行连接
    if (-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    for (;;)
    {
        // 显示首页面菜单 登录 注册 退出
        cout << "======================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "======================" << endl;
        cout << "choice:";
        int choice = 0;
        cin >> choice;
        cin.get();

        switch (choice)
        {
            case 1: // 登录业务
            {
                int id = 0;
                char pwd[50] = {0};
                cout << "userid:" << endl;
                cin >> id;
                cin.get();
                cout << "userpassword:" << endl;
                cin.getline(pwd, 50);

                json js;
                js["msgid"] = LOGIN_MSG;
                js["id"] = id;
                js["password"] = pwd;
                string request = js.dump();

                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if (len == -1)
                {
                    cerr << "send login msg error: " << request << endl;
                }
                else
                {
                    char buffer[1024] = {0};
                    len = recv(clientfd, buffer, 1024, 0);
                    if (-1 == len)
                    {
                        cerr << "recv login response error" << endl;
                    }
                    else
                    {
                        json responsejs = json::parse(buffer);
                        if (0 != responsejs["errno"].get<int>()) // 登录失败
                        {
                            cerr << responsejs["errmsg"] << endl;
                        }
                        else // 登录成功
                        {
                            // 记录当前用户的id和name
                            g_currentUser.setId(responsejs["id"].get<int>());
                            g_currentUser.setName(responsejs["name"]);

                            // 记录当前用户的好友列表信息
                            if (responsejs.contains("friends"))
                            {
                                //初始化
                                g_currentUserFriendList.clear();
                                vector<string> vec = responsejs["friends"];
                                for (string &str : vec)
                                {
                                    json js = json::parse(str);
                                    User user;
                                    user.setId(js["id"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    g_currentUserFriendList.push_back(user);
                                }
                            }

                            // 记录当前用户的群组列表信息
                            if (responsejs.contains("groups"))
                            {
                                //初始化
                                g_currentUserGroupList.clear();
                                vector<string> vec1 = responsejs["groups"];
                                for (string &groupstr : vec1)
                                {
                                    json groupjson = json::parse(groupstr);
                                    Group group;
                                    group.setId(groupjson["id"].get<int>());
                                    group.setName(groupjson["groupname"]);
                                    group.setDesc(groupjson["groupdesc"]);

                                    vector<string> vec2 = groupjson["users"];
                                    for (string &userstr : vec2)
                                    {
                                        GroupUser user;
                                        json js = json::parse(userstr);
                                        user.setId(js["id"].get<int>());
                                        user.setName(js["name"]);
                                        user.setState(js["state"]);
                                        user.setRole(js["role"]);
                                        group.getUsers().push_back(user);
                                    }

                                    g_currentUserGroupList.push_back(group);
                                }
                            }

                            // 显示登录用户的基本信息
                            showCurrentUserData();

                            // 显示当前用户的离线消息，个人聊天消息或者群组消息
                            if (responsejs.contains("offlinemsg"))
                            {
                                vector<string> vec = responsejs["offlinemsg"];
                                for (string &str : vec)
                                {
                                    json js = json::parse(str);
                                    int msgtype  = js["msgid"].get<int>();
                                    if(msgtype == ONE_CHAT_MSG)
                                    {
                                        cout<<js["time"].get<string>()<<"["<<js["id"]<<"]"<<js["name"].get<string>()<<" said: "<<js["msg"].get<string>()<<endl;
                                    }
                                    else if(msgtype == GROUP_CHAT_MSG)
                                    {
                                        cout<<"群消息["<<js["groupid"]<<"]: "<<js["time"].get<string>()<<"["<<js["id"]<<"]"<<js["name"].get<string>()<<" said: "<<js["msg"].get<string>()<<endl;
                                    }                        
                                }
                            }
                            // 登录成功，启动接收线程负责接收数据，该线程只启动一次
                            static int readthreadnumber = 0;
                            if(readthreadnumber ==0)
                            {
                                std::thread readTask(readTaskHandler, clientfd);
                                readTask.detach();
                                readthreadnumber++;
                            }
                            // 进入聊天主菜单界面
                            isMainMenuRunning = true;
                            mainMenu(clientfd);
                        }
                    }
                }
                break;
            }  
            case 2: // 注册业务
            {
                char name[50] = {0};
                char pwd[50] = {0};
                cout << "username: " << endl;
                cin.getline(name, 50);
                cout << "password: " << endl;
                cin.getline(pwd, 50);

                json js;
                js["msgid"] = REG_MSG;
                js["name"] = name;
                js["password"] = pwd;
                string request = js.dump();
                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if(len == -1)
                {
                    cerr<<"send reg msg error: "<<request<<endl;
                }
                else
                {
                    char buffer[1024] = {0};
                    len = recv(clientfd,buffer,1024,0);
                    if(-1 == len)
                    {
                        cerr<<"recv reg response error"<<endl;
                    }
                    else
                    {
                        json responsejs = json::parse(buffer);
                        if(0 != responsejs["errno"].get<int>())
                        {
                            cerr<<name<<" is already exist, register error!"<<endl;
                        }
                        else
                        {
                            cout<<name<<" register success, userid is "<<responsejs["id"]<<", do not forget it!"<<endl;
                        }
                    }
                }
                break;
            }  
            case 3: // quit业务
            {
                close(clientfd);
                exit(0);
                break;
            }   
            default:
            {
                cerr << "invalid input!" << endl;
                break;
            }
        }
    }
}                                                                                                      

// 显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout << "======================login user=====================" << endl;
    cout << "current login user => id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << endl;
    cout << "----------------------frined list----------------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    else
    {
        cout << "NO FRIEND" << endl;
    }
    cout << "----------------------group list-----------------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState() << " " << user.getRole() << endl;
            }
        }
    }
    else
    {
        cout << "NO GROUP" << endl;
    }
    cout << "=======================================================" << endl;
}

// 接收线程
void readTaskHandler(int clientfd)
{
    for(;;)
    {
        char buffer[1024] = {0};
        int len = recv(clientfd,buffer,1024,0);
        if(-1 == len || 0 == len)
        {
            close(clientfd);
            exit(-1);
        }

        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if(msgtype == ONE_CHAT_MSG)
        {
            cout<<js["time"].get<string>()<<"["<<js["id"]<<"]"<<js["name"].get<string>()<<" said: "<<js["msg"].get<string>()<<endl;
            continue;
        }
        else if(msgtype == GROUP_CHAT_MSG)
        {
            cout<<"群消息["<<js["groupid"]<<"]: "<<js["time"].get<string>()<<"["<<js["id"]<<"]"<<js["name"].get<string>()<<" said: "<<js["msg"].get<string>()<<endl;
            continue;
        }
    }
}

// 主聊天页面程序
void mainMenu(int clientfd)
{
    help();
    char buffer[1024] = {0};
    while(isMainMenuRunning)
    {
        cin.getline(buffer,1024);
        string commandbuf(buffer);
        string command;//存储命令
        int idx = commandbuf.find(':');
        if(idx == -1)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0,idx);
        }
        auto it = commandHandlerMap.find(command);
        if(it == commandHandlerMap.end())
        {
            cerr << "invalid input command!"<<endl;
            continue;
        }
        //封闭原则，添加新功能不需要修改mainMenu函数
        it->second(clientfd,commandbuf.substr(idx+1,commandbuf.size()-idx));//调用command对应的函数
    }
}

//聊天界面各个命令的实现
void help(int,string)
{
    cout<<"show command list"<<endl;
    for(auto& p : commandMap)
    {
        cout<<p.first<<" : "<<p.second<<endl;
    }
    cout<<endl;
}
void addfriend(int clientfd,string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();
    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len == -1)
    {
        cerr<<"send addfriend msg error -> "<<buffer<<endl;
    }
    else
    {
        cout<<"add friend success!"<<endl;
    }
}
void chat(int clientfd,string str)
{
    int idx = str.find(":");
    if(idx == -1)
    {
        cerr<<"chat command invalid!"<<endl;
        return;
    }
    int friendid = atoi(str.substr(0,idx).c_str());
    string message = str.substr(idx+1,str.size()-idx);
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();
    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len == -1)
    {
        cerr<<"send chat msg error -> "<<buffer<<endl;
    }
}
//creategroup:groupname:groupdesc
void creategroup(int clientfd,string str)
{
    int idx = str.find(':');
    if(idx == -1)
    {
        cerr<<"creategroup command invalid!"<<endl;
        return;
    }
    string groupname = str.substr(0,idx);
    string groupdesc = str.substr(idx+1,str.size()-idx);
    json js;
    js["msgid"] = CREAT_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();
    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len == -1)
    {
        cerr<<"send creategroup msg error -> "<<buffer<<endl;
    }
}
//addgroup:groupid
void addgroup(int clientfd,string str)
{
    int groupid = stoi(str);
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();
    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len == -1)
    {
        cerr<<"send addgroup msg error -> "<<buffer<<endl;
    }
    else
    {
        cout<<"add group success!"<<endl;
    }
}
//groupchat:groupid:message
void groupchat(int clientfd,string str)
{
    int idx = str.find(':');
    if(idx == -1)
    {
        cerr<<"groupchat command invalid!"<<endl;
        return;
    }
    int groupid = stoi(str.substr(0,idx));
    string message = str.substr(idx+1,str.size()-idx);
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();
    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len == -1)
    {
        cerr<<"send groupchat msg error -> "<<buffer<<endl;
    }
}
//loginout
void loginout(int clientfd,string str)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();
    int len = send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len == -1)
    {

        cerr<<"send loginout msg error -> "<<buffer<<endl;
    }
    else
    {
        cout<<"loginout success!"<<endl;
        isMainMenuRunning = false;
    }
}

// 获取系统时间
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm t;
    localtime_r(&tt, &t); 
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "%d-%02d-%02d %02d:%02d:%02d",
             t.tm_year + 1900,  
             t.tm_mon + 1,      
             t.tm_mday,
             t.tm_hour, 
             t.tm_min, 
             t.tm_sec);      
    return string(buf);
}
