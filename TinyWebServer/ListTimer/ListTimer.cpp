#include "ListTimer.h"

int ListTimer::ltEpollFd = -1;

ClientData::ClientData()
{
}
void ClientData::setClientSocket(const int& sock)
{
    this->clientSocket = sock;
}

void ClientData::setClientAddress(struct sockaddr_in addr)
{
    this->clientAddress = addr;
}

void ClientData::setTimer(Timer* timer)
{
    this->timer = timer;
}

Timer* ClientData::getTimer()
{
    return this->timer;
}

int ClientData::getClientSocket()
{
    return this->clientSocket;
}

struct sockaddr_in ClientData::getClientAddress()
{
    return this->clientAddress;
}

Timer::Timer() : timeout(time(NULL)), clientData(nullptr), cbFunc(callbFunc)
{
}

Timer::~Timer()
{

}

void Timer::setClientData(ClientData* cData)
{
    this->clientData = cData;
}



time_t Timer::getTimeout() const 
{
    return timeout;
}

void Timer::setTimeout(const time_t& tm)
{
    this->timeout = tm;
}

ClientData* Timer::getClientData()
{
    return clientData;
}

ListTimer::ListTimer()
{
}

ListTimer::~ListTimer()
{ 
    while(!listTimer.empty())
    {
        Timer* timer = *listTimer.begin();
        listTimer.erase(*listTimer.begin());
        delete timer;
    }
}

void ListTimer::addTimer(Timer* timer)
{
    listTimer.insert(timer);
}

void ListTimer::delTimer(Timer* timer)
{
    listTimer.erase(timer);
    delete timer;
}

void ListTimer::adjustTimer(Timer* timer, const time_t& time)
{
    //队列中某一定时器超时时间增加 往后调整队列
    listTimer.erase(timer);
    timer->setTimeout(time);
    listTimer.insert(timer);
}

void ListTimer::run()
{
    //处理到期任务
    time_t cur = time(NULL);
    while(!listTimer.empty())
    {
        Timer* timer = *listTimer.begin();
        if(timer->getTimeout() > cur)
        {
            break;
        }
        timer->cbFunc(timer->getClientData());
        //处理完从表中移除
        timer->getClientData()->setTimer(nullptr);
        delTimer(timer);
    }
}

void callbFunc(ClientData* clientData)
{
    //从epoll中移除非活动连接并关闭socket
    epoll_ctl(ListTimer::ltEpollFd, EPOLL_CTL_DEL, clientData->getClientSocket(), 0);
    assert(clientData);
    close(clientData->getClientSocket());
    Http::userCount--;
}