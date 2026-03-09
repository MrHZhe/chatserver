#include "offlinemessagemodel.hpp"
#include "db.h"

// 存储用户的离线消息
void OfflineMsgModel::insert(int userid, string msg)
{
    //组装sql语句
    char sql[1024] = {0};
    sprintf(sql,"insert into offlinemessage(userid,message) values(%d,'%s')",
            userid,msg.c_str());
    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            LOG_INFO<<"离线信息表插入成功！";
            return;
        }
    }
    LOG_INFO<<"离线信息表插入失败！";
}

// 删除用户的离线消息
void OfflineMsgModel::remove(int userid)
{
    //组装sql语句
    char sql[1024] = {0};
    sprintf(sql,"delete from offlinemessage where userid = %d",userid);
    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            LOG_INFO<<"离线信息表删除成功！";
            return;
        }
    }
    LOG_INFO<<"离线信息表删除失败！";
}

//查询用户的离线消息
vector<string> OfflineMsgModel::query(int userid)
{
    //组装sql语句
    char sql[1024] = {0};
    sprintf(sql,"select message from offlinemessage where userid = %d",userid);
    MySQL mysql;
    vector<string> vec;
    if(mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                vec.push_back(row[0]);
            }
            mysql_free_result(res);
            return vec;
        }
    }
    return vec;
}