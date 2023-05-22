#ifndef TASK_H
#define TASK_h

#include <iostream>
#include "sys/socket.h"
#include "sys/types.h"
#include "sys/epoll.h"
#include <unistd.h>
#include <errno.h>

class Task
{
public:

Task(int fd, int state);
~Task();
int getFd() { return this->fd; }
int getState() { return this->state; }
bool readMessage();
bool writeMessage();
void clientExit();

public:
static int tEpollFd;//所有对象共享次epoll文件描述符

private:
    int fd;
    int state;//状态码标志读写事件 0为读 1为写
    static int epollFd;//所有对象共享次epoll文件描述符

};

#endif