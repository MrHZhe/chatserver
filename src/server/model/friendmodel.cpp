#include "friendmodel.hpp"

//添加好友关系(好友关系表应该存在客户端，这里为了简化直接存在服务端)
void FriendModel::insert(int userid,int friendid)
{
    //组装sql语句
    char sql[1024] = {0};
    sprintf(sql,"insert into friend(userid,friendid) values(%d,%d)",
           userid,friendid);
    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            LOG_INFO<<"添加好友成功！";
        }
    }
}
//返回用户好友列表
vector<User> FriendModel::query(int userid)
{
    //组装sql语句
    char sql[1024] = {0};
    sprintf(sql,"select a.id,a.name,a.state from user a inner join friend b on b.friendid=a.id where b.userid=%d",userid);
    MySQL mysql;
    vector<User> ans;
    if(mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                User user;
                user.setId(stoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                ans.emplace_back(user);
            }
            mysql_free_result(res);//释放资源
            return ans;
        }
    }
    return ans;
}