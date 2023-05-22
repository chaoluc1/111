#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

class Sem//信号量
{
public:
    Sem();
    Sem(int num);
    ~Sem();
    bool wait();
    bool post();

private:
    sem_t mSem;
};

class Mutex//互斥锁
{
public:
    Mutex();
    ~Mutex();
    bool lock();
    bool unlock();
    pthread_mutex_t* get();

private:
    pthread_mutex_t mMutex;
};

class Cond//条件变量
{
public:
    Cond();
    ~Cond();
    bool broadcat();
    bool signal();
    bool wait();
    bool wait(pthread_mutex_t* mutex);

private:
    pthread_cond_t mCond;
    pthread_mutex_t mMutex;//用互斥锁保护条件变量

};

#endif