#include "WebServer.h"
using namespace std;

int* WebServer::pipeFd = new int[2];

WebServer::WebServer() : timeOut(false), stopServer(true)
{
    //创建与客户端对应的客户端数据对象和Http对象 //定时器相关
    this->clientDatas = new ClientData[MAXFD];
    this->users = new Http[MAXFD];

    //root文件夹路径
    char serverPath[200];
    getcwd(serverPath, 200);//获取当前工作目录

    char root[6] = "/root";
    this->root = new char[strlen(serverPath) + strlen(root) + 1];
    strcpy(this->root, serverPath);
    strcat(this->root, root);
     
}

WebServer::~WebServer() noexcept
{
    close(this->serverSocket);
    close(this->epollFdOfListen);
    close(this->epollFdOfRW);
    close(this->pipeFd[0]);
    close(this->pipeFd[1]);
    
    delete this->threadPool;
    delete this->clientDatas;
    delete this->users;
    delete this->root;
    //delete this->epEvents;
}

void WebServer::addFdToEpoll(const int& fd, const bool& et, const bool& oneShot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if(et)
    {
        event.events |= EPOLLET;        
    }
    if(oneShot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(this->epollFdOfRW, EPOLL_CTL_ADD, fd, &event);
}

void WebServer::setNonBlock(const int& fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

void WebServer::addSignal(int sig)
{
    //注册信号
    struct sigaction sigact;
    memset(&sigact, '\0', sizeof(sigact));
    sigact.sa_handler = sigHandler;
    sigfillset( &sigact.sa_mask);
    assert(sigaction(sig, &sigact, NULL) != -1);
}

void WebServer::busy(const int& fd, const char* info)
{
    if(info == nullptr)
    {
        return;
    }
    send(fd, info, strlen(info), 0);
    close(fd);
}

void WebServer::init(string user, string passWord, string dataBaseName)
{
    this->user = user;
    this->passWord = passWord;
    this->dataBaseName = dataBaseName;
}

void WebServer::writeLog()
{
    //初始化日志
    Log::getLog()->init("./ServerLog", 0, 2000, 800000, 800);
}

void WebServer::sqlPool()
{
    //初始化数据库连接池
    this->contPool = ConnectionPool::getContPool();
    this->contPool->initMySql("172.29.164.59", this->user, this->passWord, this->dataBaseName, 3306, 6);

    users->initMySql(this->contPool);

}

bool WebServer::createThreadPool()
{
    this->threadPool = new ThreadPool<Http>(this->contPool);
    if(this->threadPool != nullptr)
    {
        return true;
    }
    return false;
}

void WebServer::createServerSocket()
{
    //获取服务器socket
    this->serverSocket = socket(PF_INET, SOCK_STREAM, 0);
    //断言判断函数是否成功执行
    assert(serverSocket >= 0);
    
    //优雅的关闭连接
    struct linger tmp = {1, 1};
    setsockopt(this->serverSocket, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

    this->setNonBlock(this->serverSocket);
    //设置
    memset(&serverAddress, 0, sizeof(this->serverAddress));
    //指定地址族 ip地址 端口
    this->serverAddress.sin_family = AF_INET;
    this->serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    this->serverAddress.sin_port = htons(atoi("9999"));

    //允许重新绑定
    int flag = 1;
    setsockopt(this->serverSocket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    //命名
    int ret = 0;
    ret = bind(this->serverSocket, (sockaddr *)&this->serverAddress, sizeof(this->serverAddress));
    //断言判断函数是否成功执行
    assert(ret >= 0);
    
    //监听
    ret = listen(this->serverSocket, 5);
    //断言判断函数是否成功执行   
    assert(ret >= 0);
    
    //设置epoll
    //this->epEvents = new epoll_event[MAXEVENT];//开辟空间存放活动连接
    this->epollFdOfListen = epoll_create(5);//创建存放事件连接的文件描述符
    this->epollFdOfRW = epoll_create(5);
    //断言
    assert(this->epollFdOfListen != -1);
    assert(this->epollFdOfRW != -1);

    //同时设置Task类和ListTimer里的epollFd
    ListTimer::ltEpollFd = this->epollFdOfRW;
    Http::hEpollFd = this->epollFdOfRW;

    //注册事件
    epoll_event event;
    event.data.fd = this->serverSocket;
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    epoll_ctl(this->epollFdOfListen, EPOLL_CTL_ADD, this->serverSocket, &event);

    if(pthread_create(&this->tid, NULL, this->run, this))
    {
        throw std::exception();
    }
    //线程终止后自动销毁
    if(pthread_detach(this->tid))
    {
        throw std::exception();
    }

}

bool WebServer::createSignal()
{
    int res = socketpair(PF_UNIX, SOCK_STREAM, 0, WebServer::pipeFd);
    //断言
    assert(res != -1);
    //设置非阻塞
    this->setNonBlock(WebServer::pipeFd[0]);
    this->setNonBlock(WebServer::pipeFd[1]);
    //添加epoll读事件
    this->addFdToEpoll(WebServer::pipeFd[0], false, false);

    //添加信号
    this->addSignal(SIGALRM);
    this->addSignal(SIGTERM);

    //设置
    alarm(TIMESLOT);

    return true;
}

void WebServer::epollWait()
{
    while(this->stopServer)
    {
        int count = epoll_wait(this->epollFdOfListen, &this->listenEvent, 1, -1);
        if(count == -1 && errno != EINTR)
        {
            //LOG_ERROR("WebServer.cpp:185 /s", "epoll failure");   
            break;
        }
        //遍历所有活跃连接
        if(count == 1)
        {
            int sockFd = this->listenEvent.data.fd;
            if(sockFd == this->serverSocket)
            {
                //服务器接收到连接请求
                bool flag = this->dealWithClientLink();
                if(!flag)
                {
                    //表示处理完所有连接或服务器繁忙
                    continue;
                }
            }
        }
    }
}



bool WebServer::dealWithClientLink()
{
    //处理客户端连接
    sockaddr_in clientAddresss;
    socklen_t clientSize = sizeof(clientAddresss);
    //循环处理
    while(true)
    {
        int clientSocket = accept(this->serverSocket, (sockaddr *)&clientAddresss, &clientSize);
        if(clientSocket < 0)
        {
            //LOG_ERROR("WebServer.cpp:248 %s:errno is %d", "accept error", errno);
            return false;
        
        }
        if(Http::userCount >= MAXFD)
        {
            //暂时处理不过来
            this->busy(clientSocket, "Internal server busy");
            //LOG_ERROR("WebServer.cpp:256 %s", "Internal server busy");
            return false;
        }
        //LOG_INFO("WebServer.cpp:259 client:%d%s", clientSocket, "connect");
        setNonBlock(clientSocket);        
        //设置Http
        this->users[clientSocket].init(clientSocket, clientAddresss, this->root, this->user, this->passWord, this->dataBaseName);
        //设置定时器
        this->setTimer(clientAddresss, clientSocket); 
        //最后往epoll例程中注册
        this->addFdToEpoll(clientSocket, true, true);

    }
    return true;
}

void WebServer::delTimer(Timer *timer, int sockfd)
{
    //直接调用回调函数终止连接
    timer->cbFunc(&clientDatas[sockfd]);
    if (timer)
    {
        this->listTimer.delTimer(timer);
    }

    //LOG_INFO("WebServer.cpp:280 close fd %d", clientDatas[sockfd].getClientSocket());
}

void WebServer::dealWithRead(const int& fd)
{
    //可读事件
    //有数据传输 延长超时时间 调整在定时器链表中的位置    
    this->adjustTimer(fd);    
    //将其加入任务队列    
    threadPool->push(this->users + fd, 0);
    while(true)
    {
        if(this->users[fd].improv == 1)
        {
            if(this->users[fd].timerFlag == 1)
            {
                //请求报文错误或客户端退出 断开连接
                //LOG_INFO("WebServer.cpp:301 client exit");
                this->delTimer(this->clientDatas[fd].getTimer(), fd);
                this->users[fd].timerFlag = 0;
            }
            this->users[fd].improv = 0;
            break;
        }
    }
}

void WebServer::dealWithWrite(const int& fd)
{
    //可写事件
    this->adjustTimer(fd);    
    //将其加入任务队列    
    threadPool->push(this->users + fd, 1);
    while(true)
    {
        if(this->users[fd].improv == 1)
        {
            if(this->users[fd].timerFlag == 1)
            {
                //请求报文错误 断开连接
                this->delTimer(this->clientDatas[fd].getTimer(), fd);
                this->users[fd].timerFlag = 0;
            }
            this->users[fd].improv = 0;
            break;
        }
    }
}

void WebServer::sigHandler(int sig)
{
    //保留原来的errno，在函数最后恢复，以保证函数的可重入性 ？
    int saveErrno = errno;
    int msg = sig;
    send(pipeFd[1], (char*)&msg, 1, 0);
    errno = saveErrno;
}

bool WebServer::dealWithSignal(const int& fd)
{
    char signals[1024];
    int res = recv(fd, signals, sizeof(signals), 0);
    if(res <= 0)
    {
        return false;
    }
    else
    {
        //每个信号占1字节 逐个字节接收信号
        for(int i = 0; i < res; i++)
        {
            switch(signals[i])
            {
                case SIGALRM:
                {
                    this->timeOut = true;//表示超时
                    break;
                }
                case SIGTERM:
                {
                    this->stopServer = true;//关闭服务器
                    break;
                }
            }
        }
    }
    return true;
}

void WebServer::timerHandler()
{
    //处理超时任务
    listTimer.run();
    //重新设置
    //LOG_INFO("WebServer.cpp:374 TimeOut!");
    alarm(TIMESLOT);

}

bool WebServer::setTimer(struct sockaddr_in clientAddresss, const int& clientSocket)
{
    this->clientDatas[clientSocket].setClientSocket(clientSocket);
    this->clientDatas[clientSocket].setClientAddress(clientAddresss);
    Timer* timer = new Timer;
    timer->setClientData(&clientDatas[clientSocket]);
    timer->setTimeout(time(NULL) + this->TIMESLOT * 3);
    clientDatas[clientSocket].setTimer(timer);
    this->listTimer.addTimer(timer);    
    return true;
}

void WebServer::adjustTimer(const int& fd)
{
    time_t cur = time(NULL);
    listTimer.adjustTimer(clientDatas[fd].getTimer(), cur + TIMESLOT * 3);
    //LOG_INFO("WebServer.cpp:399 %s", "adjust timer once");
}

void* WebServer::run(void* arg)
{
    WebServer* wbSer = (WebServer*)arg;
    wbSer->work();
    return wbSer;
}

void WebServer::work()
{
    while(this->stopServer)
    {   
        
        int count = epoll_wait(this->epollFdOfRW, this->epEvents, MAXEVENT, -1);      
        if(count == -1 && errno != EINTR)
        {
            //LOG_ERROR("WebServer.cpp:185 /s", "epoll failure");   
            break;
        }
        //遍历所有活跃连接
        for(int i = 0; i < count; i++)
        {
            int sockFd = this->epEvents[i].data.fd;
            // if(this->epEvents[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            // {
            //     //服务器端关闭连接
            //     Timer* timer = this->clientDatas[sockFd].getTimer();
            //     this->delTimer(timer, sockFd);
                
            // }
            if((sockFd == this->pipeFd[0]) && (this->epEvents[i].events & EPOLLIN))
            {
                //处理信号
                bool flag = this->dealWithSignal(sockFd);
            }
            else if(epEvents[i].events & EPOLLIN)
            {
                //读事件
                this->dealWithRead(sockFd);
            }
            else if(epEvents[i].events & EPOLLOUT)
            {
                //写事件
                this->dealWithWrite(sockFd);              
            }

        }
        //当所有活动连接处理完再处理超时任务
        if(timeOut)
        {
            this->timerHandler();
            //LOG_INFO("WebServer.cpp:230 %s", "timer tick");
            timeOut = false;
        }

    }    
}