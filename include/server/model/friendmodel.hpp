#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H

#include "user.hpp"
#include "db.h"
#include<vector>
#include<string>
using namespace std;
//维护好友信息的操作接口方法
class FriendModel
{
public:
    //添加好友关系(好友关系表应该存在客户端，这里为了简化直接存在服务端)
    void insert(int userid,int friendid);
    //返回用户好友列表
    vector<User> query(int userid);
};


#endif