#ifndef SQL_H
#define SQL_H

#include <iostream>
#include <cstdio>
#include <mysql/mysql.h>
#include <list>
#include <string>
#include "/home/zuizuidac1/socket/TinyWebServer/Log/Log.h"
//单例模式创建连接池管理数据库连接

class ConnectionPool
{
public: 
    ConnectionPool(const ConnectionPool& contPool) = delete;
    ConnectionPool operator=(const ConnectionPool& contPool) = delete;
    static ConnectionPool* getContPool()//获取指向唯一一个连接池对象的指针
    {
        static ConnectionPool contPool;
        return &contPool;
    }

    //初始化
    void initMySql(std::string url, std::string user, std::string passWord, std::string databaseName, int port, int MaxCont);
    //获取连接
    MYSQL* getCont();

    //释放连接
    bool releaseCont(MYSQL* cont);


private:
    ConnectionPool();
    ~ConnectionPool();

    std::list<MYSQL*> pool;//连接池

    //锁机制
    Mutex mutex;
    Sem sem;

    int maxCont; //最大连接数
    int curCont; //已连接数
    int freeCont;//空闲连接数

	std::string url;//主机地址
    std::string port;//数据库端口号
	std::string user;//登陆数据库用户名
	std::string passWord;//登陆数据库密码
	std::string databaseName;//使用数据库名



    //销毁连接    
    void destroyPool();
};

class ConnectionRAII
{
    //RAII技法管理数据库连接
public:
    ConnectionRAII(MYSQL** mysql, ConnectionPool* connectPool);
    ~ConnectionRAII();

private:
    ConnectionPool* contPool;
    MYSQL* cont;
};


#endif