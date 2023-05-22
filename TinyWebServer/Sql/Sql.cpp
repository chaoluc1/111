#include "Sql.h"
using namespace std;

ConnectionPool::ConnectionPool() : curCont(0), freeCont(0), maxCont(0)
{}

ConnectionPool::~ConnectionPool()
{
    destroyPool();
}


void ConnectionPool::initMySql(std::string url, std::string user, std::string passWord, std::string databaseName, int port, int MaxCont)
{
    this->url = url;
    this->port = port;
    this->user = user;
    this->passWord = passWord;
    this->databaseName = databaseName;

    //循环建立数据库连接
    for(int i = 0; i < MaxCont; i++)
    {
        MYSQL* cont = nullptr;
        cont = mysql_init(cont);
        if(cont == nullptr)
        {
        	//LOG_ERROR("Sql.cpp:29 MySQL Error %s", mysql_error(cont));
			exit(1);
        }

        if(mysql_real_connect(cont, url.c_str(), user.c_str(), passWord.c_str(), databaseName.c_str(), port, NULL, 0) == nullptr)
        {
			//LOG_ERROR("Sql.cpp:37 MySQL Error %s", mysql_error(cont));
			exit(1);
        }

        this->pool.push_back(cont);
        freeCont++;
    }

    //设置信号量
    this->sem = Sem(freeCont);
    this->maxCont = freeCont;

}

MYSQL* ConnectionPool::getCont()
{
    MYSQL* cont = nullptr;

    if(this->pool.empty())
    {
        return nullptr;
    }

    this->sem.wait();
    this->mutex.lock();

    //取出尾连接并返回
    cont = this->pool.back();
    this->pool.pop_back();

    this->freeCont--;
    this->curCont++;

    this->mutex.unlock();

    return cont;

}

bool ConnectionPool::releaseCont(MYSQL* cont)
{
    if(cont == nullptr)
    {
        return false;
    }

    mutex.lock();

    //连接放回表
    pool.push_back(cont);
    freeCont++;
    curCont--;

    mutex.unlock();

    sem.post();

    return true;
}

void ConnectionPool::destroyPool()
{
    mutex.lock();
    if(!pool.empty())
    {
        list<MYSQL*>::iterator it = pool.begin();
        while(it != pool.end())
        {
            MYSQL* cont = *it;
            mysql_close(cont);
        }
        curCont = 0;
        freeCont = 0;
        
        pool.clear();
    }
    mutex.unlock();
}


ConnectionRAII::ConnectionRAII(MYSQL** mysql, ConnectionPool* connectPool)
{
    //构造即获取连接
    *mysql = connectPool->getCont();
    this->cont = *mysql;
    this->contPool = connectPool;
}

ConnectionRAII::~ConnectionRAII()
{
    //析构即释放连接
    contPool->releaseCont(cont);
}