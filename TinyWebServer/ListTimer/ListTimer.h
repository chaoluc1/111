#ifndef LISTTIMER_H
#define LISTTIMER_H


#include <iostream>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <set>
#include <sys/epoll.h>
#include <unistd.h>
#include <cassert>
#include "/home/zuizuidac1/socket/TinyWebServer/Http/Http.h"


class Timer;
//用户数据 用于与定时器绑定
class ClientData
{
public:
    ClientData();
    void setClientSocket(const int& socket);
    void setClientAddress(struct sockaddr_in addr);
    void setTimer(Timer* timer);
    Timer* getTimer();
    int getClientSocket();
    struct sockaddr_in getClientAddress();
private:

    struct sockaddr_in clientAddress;//地址
    int clientSocket;//文件描述符
    Timer* timer;//绑定的定时器
};


//定时器
class Timer
{
public:
    Timer();
    ~Timer();
    
    time_t getTimeout() const;
    ClientData* getClientData();
    void (*cbFunc)(ClientData*);//任务回调函数
    void setClientData(ClientData* cData);
    void setTimeout(const time_t& tm);

private:
    time_t timeout;//超时时间 采用绝对时间
    ClientData* clientData;//用户数据
};

class MyCompare
{
public:
    bool operator()(const Timer* t1, const Timer* t2) const
    {
        return t1->getTimeout() < t2->getTimeout();
    }
};

//定时器管理
//用set容器管理
//升序实现
class ListTimer
{
public:
    ListTimer();
    ~ListTimer();

    void addTimer(Timer* timer);
    void delTimer(Timer* timer);
    void adjustTimer(Timer* timer, const time_t& time);
    void run();

    static int ltEpollFd;
private:

    std::multiset<Timer*, MyCompare> listTimer;

};

void callbFunc(ClientData* clientData);




#endif