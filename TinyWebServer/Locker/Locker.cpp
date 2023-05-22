#include "Locker.h"

Sem::Sem()
{
    //初始化信号量
    if(sem_init(&mSem, 0, 0) != 0)
    {
        //初始化失败 抛出异常
        throw std::exception();
    }
}
Sem::Sem(int num)
{
    if(sem_init(&mSem, 0, num) != 0)
    {
        throw std::exception();
    }
}
Sem::~Sem()
{
    //销毁
    sem_destroy(&mSem);
}
bool Sem::wait()
{
    //等待信号量 信号量值-1 如果信号量的值为0将阻塞
    return sem_wait(&mSem) == 0;//函数成功时返回0
}
bool Sem::post()
{
    //信号量值+1 当信号量值大于0时，其它阻塞在wait等待信号量的线程将被唤醒
    return sem_post(&mSem) == 0;//函数成功时返回0
}


Mutex::Mutex()
{
    if(pthread_mutex_init(&mMutex, NULL) != 0)
    {
        //初始化失败
        std::exception();
    }
}
Mutex::~Mutex()
{
    //销毁互斥锁
    pthread_mutex_destroy(&mMutex);
}
bool Mutex::lock()
{
    //加锁 已被锁上将阻塞
    return pthread_mutex_lock(&mMutex) == 0;//函数成功时返回0
}
bool Mutex::unlock()
{
    //解锁
    return pthread_mutex_unlock(&mMutex) == 0;//函数成功时返回0
}

pthread_mutex_t* Mutex::get()
{
    return &mMutex;
}

Cond::Cond()
{
    //先初始化互斥锁
    if(pthread_mutex_init(&mMutex, NULL) != 0)
    {
        //初始化失败
        std::exception();
    }
    if(pthread_cond_init(&mCond, NULL) != 0)
    {
        //初始化失败
        //互斥锁已经初始化完成 需要销毁
        pthread_mutex_destroy(&mMutex);
        std::exception();
    }
}
Cond::~Cond()
{
    //销毁
    pthread_mutex_destroy(&mMutex);
    pthread_cond_destroy(&mCond);
}
bool Cond::broadcat()
{
    //唤醒所有等待目标条件变量的线程
    return pthread_cond_broadcast(&mCond) == 0;//函数成功时返回0
}
bool Cond::signal()
{
    //唤醒一个等待目标条件变量的线程
    return pthread_cond_signal(&mCond) == 0;//函数成功时返回0
}



bool Cond::wait()
{
    //等待条件变量
    int result = 0;
    pthread_mutex_lock(&mMutex);
    result = pthread_cond_wait(&mCond, &mMutex);//函数成功时返回0
    pthread_mutex_unlock(&mMutex);
    return result == 0;
}

bool Cond::wait(pthread_mutex_t* mutex)
{
    int result = 0;
    result = pthread_cond_wait(&mCond, mutex);
    return result == 0;

}