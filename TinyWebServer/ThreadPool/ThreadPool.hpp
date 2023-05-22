#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include <iostream>
#include <pthread.h>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <list>
#include "/home/zuizuidac1/socket/TinyWebServer/Sql/Sql.h"
using namespace std;

#define threadNum 10

template <typename T>
class ThreadPool
{
private:

static void* run(void* arg);

public:
ThreadPool(ConnectionPool* ctPool);
~ThreadPool();
bool push(T* request, int state);

private:
    pthread_t* threads;

    Mutex mutex;
    Sem sem;
    Cond cond;

    std::list<T*> taskQueue;
    int queueSize;//队列容量

    ConnectionPool* contPool;

    void work();

};

template<typename T>
ThreadPool<T>::ThreadPool(ConnectionPool* ctPool) : queueSize(10000), contPool(ctPool), threads(nullptr)
{
    this->threads = new pthread_t[threadNum];
    //创建线程
    for(int i = 0; i < threadNum; i++)
    {
        if(pthread_create(&this->threads[i], NULL, run, this))
        {
            delete[] this->threads;
            throw std::exception();
        }
        //线程终止后自动销毁
        if(pthread_detach(this->threads[i]))
        {
            delete[] this->threads;
            throw std::exception();
        }
    }
}

template<typename T>
ThreadPool<T>::~ThreadPool()
{
    delete[] this->threads;
}

template<typename T>
void *ThreadPool<T>::run(void *arg)
{
    ThreadPool* self = (ThreadPool*)arg;    
    self->work();
    return self;
}

template<typename T>
void ThreadPool<T>::work()
{

    //等待唤醒
    while(true)
    {
        this->sem.wait();
        //唤醒后上锁 
        this->mutex.lock();
        if(this->taskQueue.empty())
        {
            //空 则继续等待
            this->mutex.unlock();
            std::cout << "continue" << std::endl;
            continue;
        }
        //取出队头任务
        T* request = this->taskQueue.front();
        this->taskQueue.pop_front();
        this->mutex.unlock();
        //处理任务
        //根据状态判断是读事件还是写事件
        if(request == nullptr)
        {
            continue;
        }
        if(request->state == 0)
        {
            //读事件
            if(request->readData())
            {
                //读取成功
                request->improv = 1;
                ConnectionRAII mysqlCon(&request->mysql, this->contPool);
                //分析
                request->phrase();
            }
            else
            {
                //客户端退出
                request->improv = 1;
                request->timerFlag = 1;
            }
        }
        else
        {
            //写事件
            if(request->response())
            {
                request->improv = 1;
            }
            else
            {
                request->improv = 1;
                request->timerFlag = 1;
            }
        }
        
    }
}

template<typename T>
bool ThreadPool<T>::push(T* request, int state)
{
    //加锁 将新的任务加入任务队列
    this->mutex.lock();
    if(taskQueue.size() > queueSize)
    {
        this->mutex.unlock();
        //LOG_ERROR("ThreadPool.hpp:148 push error");
        return false;
    }
    request->state = state;
    taskQueue.push_back(request);
    this->mutex.unlock();
    //唤醒线程处理任务
    this->sem.post();
    return true;
}

#endif