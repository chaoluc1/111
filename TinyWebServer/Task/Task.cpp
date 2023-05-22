#include "Task.h"

int Task::tEpollFd = -1;

Task::Task(int fd, int state) : fd(fd), state(state) {}

Task::~Task() {}

bool Task::readMessage()
{
    //读事件
    char message[1024];
    int messageLen = 0;
    while(true)
    {
        if((messageLen = recv(fd, message, 1024, 0)) != -1)
        {
            if(messageLen == 0)
            {
                //客户端退出
                clientExit();
                return true;             
            }
            else
            {
                if(messageLen < 1024)
                {
                    message[messageLen] = '\0';    
                }
                std::cout << "get message from client " << fd << ": " << message << std::endl;
                //回声
                if(send(fd, message, messageLen, 0) != -1)
                {    
                }
                else
                {
                    std::cout << "send error " << std::endl;
                    int mErrno = errno;
                    std::cout << "error: " << mErrno << std::endl;
                    return false;
                }                        
            }
        }
        else
        {
            if(errno == EAGAIN)
            {
                epoll_event event;
                event.data.fd = fd;
                event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                epoll_ctl(Task::tEpollFd, EPOLL_CTL_MOD, fd, &event);
                return true;
            }
            std::cout << "recv error " << std::endl;
            int mErrno = errno;
            std::cout << "error: " << mErrno << std::endl;
            return false; 
        }
    }
}

bool Task::writeMessage()
{
    //写事件
    return true;
}

void Task::clientExit()
{
    //客户端断开连接
    //从epoll例程中删除对事件的关注
    epoll_ctl(Task::tEpollFd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    std::cout << "client " << fd << " exit " << std::endl;
}