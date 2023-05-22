#include "WebServer/WebServer.h"
using namespace std;



int main()
{
    string user{"root"};
    string passWord{"woyeshizuile"};
    string dataBaseName{"user"};

    WebServer webServer;//构造WebServer对象

    webServer.init(user, passWord, dataBaseName);//初始化

    //webServer.writeLog();//日志系统

    webServer.sqlPool();//数据库池

    webServer.createThreadPool();//线程池

    webServer.createServerSocket();//监听

    webServer.createSignal();//信号 

    webServer.epollWait();//处理连接
    
    return 0;
}