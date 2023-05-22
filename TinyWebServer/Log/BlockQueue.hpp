#ifndef BLOCKQUEUE_H
#define BLOCKQUQUE_H

#include <list>
#include "/home/zuizuidac1/socket/TinyWebServer/Locker/Locker.h"

template<typename T>
class BlockQueue
{
public:
    BlockQueue();
    BlockQueue(int num);
    ~BlockQueue();

    void clear();
    bool push(const T& temp);
    bool pop(T& item);
    bool full();


private:
   std::list<T> blockQueue; 

    Mutex mutex;
    Cond cond;

    int capacity;//容量

};



template<typename T>
BlockQueue<T>::BlockQueue(int num) : capacity(num)
{

}

template<typename T>
BlockQueue<T>::~BlockQueue()
{

}

template<typename T>
void BlockQueue<T>::clear()
{
    mutex.lock();
    blockQueue.clear();
    mutex.unlock();
}

template<typename T>
bool BlockQueue<T>::push(const T& temp)
{
    mutex.lock();
    if(blockQueue.size() >= capacity)
    {
        cond.broadcat();
        mutex.unlock();
        return false;
    }
    //插入队尾
    blockQueue.push_back(temp);

    cond.broadcat();
    mutex.unlock();
    return true;

}

template<typename T>
bool BlockQueue<T>::pop(T& item)
{
    mutex.lock();
    while(blockQueue.size() <= 0)
    {   
        if(!cond.wait(mutex.get()))
        {
            mutex.unlock();
            return false;
        }
    }

    item = blockQueue.front();
    blockQueue.pop_front();
    mutex.unlock();
    return true;

}

template<typename T>
bool BlockQueue<T>::full()
{
    mutex.lock();
    if(blockQueue.size() >= capacity)
    {
        mutex.unlock();
        return true;
    }

    mutex.unlock();
    return false;
}



#endif