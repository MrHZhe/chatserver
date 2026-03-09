#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "group.hpp"

#include <vector>
#include <string>
using namespace std;

//维护群组信息的操作类
class GroupModel
{
public:
    //创建群组
    bool creatGroup(Group &group);
    //加入群组
    void addGroup(int userid,int groupid, string role);
    //查询用户所在群组信息
    vector<Group> queryGroups(int userid);
    //查询群组中的用户id,除了自己的id，给这些人发送群消息
    vector<int> queryGroupUsers(int userid,int groupid);
};

#endif