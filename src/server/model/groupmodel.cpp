#include "groupmodel.hpp"
#include "db.h"

//创建群组
bool GroupModel::creatGroup(Group &group)
{
    //组装sql语句
    char sql[1024] = {0};
    sprintf(sql,"insert into allgroup(groupname,groupdesc) values('%s','%s')",
           group.getName().c_str(),group.getDesc().c_str());
    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            group.setId(mysql_insert_id(mysql.getConnection()));
            LOG_INFO<<"创建群组成功！";
            return true;
        }
    }
    return false;
}

//加入群组
void GroupModel::addGroup(int userid,int groupid, string role)
{
    //组装sql语句
    char sql[1024] = {0};
    sprintf(sql,"insert into groupuser(userid,groupid,grouprole) values(%d,%d,'%s')",
           userid,groupid,role.c_str());
    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            LOG_INFO<<"加入群组成功！";
        }
    }
}

//查询用户所在群组信息
vector<Group> GroupModel::queryGroups(int userid)
{
    //组装sql语句
    //联合查询提高效率
    char sql[1024] = {0};
    sprintf(sql,"select a.id,a.groupname,a.groupdesc from allgroup a inner join groupuser b on a.id = b.groupid where b.userid = %d",
           userid);
    MySQL mysql;
    vector<Group> ans;
    if(mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                Group temp;
                temp.setId(stoi(row[0]));
                temp.setName(row[1]);
                temp.setDesc(row[2]);
                ans.emplace_back(temp);
            }
            mysql_free_result(res);//释放资源
        }
    }
    for(Group &group : ans)
    {
        sprintf(sql,"select a.id,a.name,a.state,b.grouprole from user a inner join groupuser b on a.id=b.userid where b.groupid=%d",group.getId());
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res)) != nullptr)
            {
                GroupUser user;
                user.setId(stoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                user.setRole(row[3]);
                group.getUsers().push_back(user);
            }
            mysql_free_result(res);
        }
    }
    return ans;
}

//查询群组中的用户id,除了自己的id，以便后续给这些人发送群消息
vector<int> GroupModel::queryGroupUsers(int userid,int groupid)
{
    //组装sql语句
    char sql[1024] = {0};
    sprintf(sql,"select userid from groupuser where groupid=%d and userid!=%d",
           groupid,userid);
    MySQL mysql;
    vector<int> ans;
    if(mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                ans.push_back(stoi(row[0]));
            }
            mysql_free_result(res);//释放资源
        }
    }
    return ans;
}