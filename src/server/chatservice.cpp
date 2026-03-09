#include "chatservice.hpp"
#include "public.hpp"
#include "user.hpp"
#include "muduo/base/Logging.h"
#include <string>
#include <mutex>
#include <vector>
using namespace muduo;
using namespace std;
// 获取单例对象的接口函数
ChatService *ChatService::instance()
{
    static ChatService cs;
    return &cs;
}

// 构造方法
ChatService::ChatService()
{
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});

    _msgHandlerMap.insert({CREAT_GROUP_MSG, std::bind(&ChatService::creatGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    //连接redis服务器
    if(_redis.connect())
    {
        //设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}


// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end())
    {
        // 返回一个默认处理器，空操作
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp)
        {
            LOG_ERROR << "msgid: " << msgid << " can not find handler!";
        };
    }
    else
        return _msgHandlerMap[msgid];
}

// 处理登录业务
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"];
    string pwd = js["password"];
    User user = _userModel.query(id);
    if (user.getId() == id && user.getPwd() == pwd)
    {
        if (user.getState() == "online")
        {
            // 不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "该账号已经登录！";
            conn->send(response.dump());
        }
        else
        {
            // 记录用户连接信息
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }
            //向redis订阅channel(id)
            _redis.subscribe(id);
            // 登录成功，更新用户状态信息
            user.setState("online");
            _userModel.updateState(user);
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();
            // 查询用户是否有离线信息
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty())
            {
                response["offlinemsg"] = vec;
                //读取完离线消息后，删除，防止下次登录重复读取
                _offlineMsgModel.remove(id);
            }
            //查询用户的好友信息
            vector<User> userVec = _friendModel.query(id);
            if(!userVec.empty())
            {
                vector<string> vec2;
                for(User &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }
            //查询用户的群组信息
            vector<Group> userGroupVec = _groupModel.queryGroups(id);
            if(!userGroupVec.empty())
            {
                vector<string> groupVec;
                for(Group &group : userGroupVec)
                {
                    json groupjson;
                    groupjson["id"] = group.getId();
                    groupjson["groupname"] = group.getName();
                    groupjson["groupdesc"] = group.getDesc();
                    vector<string> userVec;
                    for(GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userVec.push_back(js.dump());
                    }
                    groupjson["users"] = userVec;
                    groupVec.push_back(groupjson.dump());
                }
                response["groups"] = groupVec;
            }
            conn->send(response.dump());
        }
    }
    else
    {
        // 用户名或密码错误
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "用户名或密码错误！";
        conn->send(response.dump());
    }
}

// 处理注册业务 name password
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    if (state)
    {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}

// 处理一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toId = js["toid"].get<int>(); // 获取接收者id
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toId);
        if (it != _userConnMap.end())
        {
            // 对方在线，转发消息
            it->second->send(js.dump());
            return;
        }
    }
    User user = _userModel.query(toId);
    if(user.getState() == "online")
    {
        // 发布消息到接收者channel(toId)
        _redis.publish(toId, js.dump());
        return;
    }
    // 对方不在线
    _offlineMsgModel.insert(toId, js.dump());
}

//处理添加好友业务
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    //存储好友信息
    _friendModel.insert(userid,friendid);
    _friendModel.insert(friendid,userid);
}    

//创建群组业务
void ChatService::creatGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    //存储新创建的群组信息
    Group group(-1,name,desc);
    if(_groupModel.creatGroup(group))
    {
        //存储群组创建人信息
        _groupModel.addGroup(userid,group.getId(),"creator");
    }
}

//加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid,groupid,"normal");
}

//群聊业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> userVec = _groupModel.queryGroupUsers(userid,groupid);
    
    for(int id : userVec)
    {
        bool isLocalOnline = false;
        
        // 1. 先去本机的连接表里找
        {
            lock_guard<mutex> lock(_connMutex);
            auto it = _userConnMap.find(id);
            if(it != _userConnMap.end())
            {
                // 该群友在本机在线，直接转发群消息
                it->second->send(js.dump());
                isLocalOnline = true;
            }
        }
        
        // 2. 如果本机没有连接，判断是否在其他服务器上在线
        if (!isLocalOnline)
        {
            User user = _userModel.query(id);
            if(user.getState() == "online")
            {
                // 该群友在其他服务器上在线，通过 redis 发布消息
                _redis.publish(id, js.dump());
            }
            else
            {
                // 该群友确实离线，存储离线群消息
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}

//注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        _userConnMap.erase(userid);
    }

    // 取消订阅用户channel(id)
    _redis.unsubscribe(userid);

    User user(userid,"","","offline");
    _userModel.updateState(user);
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
        {
            if (it->second == conn)
            {
                // 删除用户的连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }
    // 取消订阅用户channel(id)
    _redis.unsubscribe(user.getId());
    if (user.getId() != -1)
    {
        // 更新用户的状态信息
        user.setState("offline");
        _userModel.updateState(user);
        LOG_INFO << "客户端退出后，用户 "<<user.getId()<<" 成功切换为离线";
    }
}

//服务器异常，业务重置
void ChatService::reset()
{
    //把所有online状态的用户设置为offline
    _userModel.resetState();
}

// 从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    // 存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}