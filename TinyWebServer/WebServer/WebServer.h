#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string>
#include <stdio.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <cassert>
#include "/home/zuizuidac1/socket/TinyWebServer/ThreadPool/ThreadPool.hpp"
#include "/home/zuizuidac1/socket/TinyWebServer/ListTimer/ListTimer.h"

const int MAXFD = 65538;//最大文件描述符数
const int MAXEVENT = 10000;//事件

class WebServer
{
public:
    WebServer();
    ~WebServer() noexcept;

    void init(std::string user, std::string passWord, std::string dataBaseNaem);//初始化

    void writeLog();//加载日志    
    void sqlPool();//加载数据库
    bool createThreadPool();//加载线程池
    void createServerSocket();//epoll监听
    bool createSignal();//信号
    void epollWait();    






private:
    int serverSocket;//服务器socket

    sockaddr_in serverAddress;//服务器地址

    int epollFd;//epoll文件描述符

    epoll_event epEvents[MAXEVENT];//存放活动连接的地址

    ThreadPool<Http>* threadPool;

    static int* pipeFd;//管道 用于统一信号源

    ListTimer listTimer;//定时器链表

    const int TIMESLOT = 300;//定义超时时间

    bool timeOut;//timeOut超时处理标志

    bool stopServer;//停止服务器运行

    ClientData* clientDatas;//用户数据

    Http* users;//用户

    char* root;//工作目录

    //数据库
    ConnectionPool* contPool;
    std::string user;
    std::string passWord;
    std::string dataBaseName;

    static void sigHandler(int sig);
    void setNonBlock(const int& fd);
    void addFdToEpoll(const int& fd, const bool& et, const bool& oneShot);
    void addSignal(int sig);    
    void busy(const int& fd, const char* info);
    bool setTimer(struct sockaddr_in clientAddresss, const int& clientSocket);
    void delTimer(Timer *timer, int sockfd);

    bool dealWithClientLink();
    void dealWithRead(const int& fd);
    void dealWithWrite(const int& fd);
    bool dealWithSignal(const int& fd);
    void adjustTimer(const int&fd);
    void timerHandler();
};

#endif