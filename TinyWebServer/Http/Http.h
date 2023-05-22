#ifndef HTTP_H
#define HTTP_H

#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <string>
#include <array>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/uio.h>
#include <map>
#include "/home/zuizuidac1/socket/TinyWebServer/Sql/Sql.h"


#define FILENAME_LEN 200
#define BUFFER_MAXSIZE 2048
#define WRITE_BUFFER_SIZE 1024

//GET /index_2013.php HTTP/1.1\r\n
//Host: www.hootina.org\r\n
//Connection: keep-alive\r\n
//Upgrade-Insecure-Requests: 1\r\n
//User-Agent: Mozilla/5.0 (Windows NT 6.1; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/65.0.3325.146 Safari/537.36\r\n
//Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8\r\n
//Accept-Encoding: gzip, deflate\r\n
//Accept-Language: zh-CN,zh;q=0.9,en;q=0.8\r\n
//\r\n




class Http
{
public:
    Http();
    ~Http();
    static int userCount;//用户数
    static int hEpollFd;

    MYSQL* mysql;
    
    int state;//读为0 写为1
    int timerFlag;//客户端是否退出的标志
    int improv;//工作线程完成的标志

    //请求方法 暂时只用GET和POST两种
    enum METHOD {GET = 0, POST};
    //主状态机状态 分别解析请求行、请求头、请求内容
    enum CHECK_STATE {CHECK_STATE_REQUESTLINE=0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT};
    //报文解析结果 //请求不完整 获取请求 错误请求 无资源        文件请求 网络错误 关闭连接
    enum HTTP_CODE{NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST,INTERNAL_ERROR, CLOSED_CONNECTION};
    //从状态机状态
    enum LINE_STATUS{LINE_OK=0, LINE_BAD, LINE_OPEN};

    //关于fd

    //接口函数
    void phrase();
    bool response();    

    //解析函数
    bool readData();//读取请求放入data中

    //响应请求
    HTTP_CODE doRequest();

    void init(int sock, struct sockaddr_in addr, char* root, std::string user, std::string passWord, std::string dataBaseName);//初始化
    void initMySql(ConnectionPool* contPool);

private:

    char data[BUFFER_MAXSIZE];//指向等待解析的数据

    //用于从状态机
    int checkIndex;//记录未解析数据的初始位置
    int readIndex;//记录未解析数据结尾的下一位置

    //用于分析请求
    int phraseIndex;//记录未分析数据的起始位置 即每一行的开头
    CHECK_STATE check = CHECK_STATE_REQUESTLINE;

    //用于标识用户
    int clientSocket;
    struct sockaddr_in clientAddress;

    //用于保存请求中分析出的数据
    char* url;
    char* version;

    int requestMethod;//0表示GET请求 1表示POST请求 初始化为0
    bool longConnection;//标识是否为长连接 初始化为false
    char* hostName;//主机名
    int contentLength;//内容长度 初始化为0
    char* userData;//保存用户提交的用户名密码
    int cgi;//判断是否是post请求

    //用于响应请求
    char file[FILENAME_LEN];//保存资源目录

    struct stat fileStat;//保存文件属性
    char* fileAddress;//文件地址

    int writeIndex;//指向未写缓冲区的起始位置
    char writeBuf[WRITE_BUFFER_SIZE];//写缓冲区

    struct iovec iv[2];//IO向量
    int ivCount;

    int bytesToSend;//写缓冲区未发送数据大小
    int bytesHaveSend;//写缓冲区已发送数据大小

    char* docRoot;


    //日志
    int closeLog;

    //数据库

    static std::map<std::string, std::string> users;//用户名和密码

    char sqlUser[20];
    char sqlPassWord[20];
    char sqlName[20];

    Mutex mutex;

    //初始化
    void init();
    void modFd(const int& ev);
    //分析
    LINE_STATUS phraseLine();//解析数据找出一行完整数据
    HTTP_CODE phraseRequest();//分析请求
    HTTP_CODE readRequestLine(char* text);//解析请求行
    HTTP_CODE readHeader(char* text);//解析请求头
    HTTP_CODE readContent(char* text);//解析请求内容    
    void closeConnect();

    //响应请求
    bool writeRequest(HTTP_CODE code); 
    bool writeCommon(const char* format, ...);//统一调用此函数将数据写入写缓冲区
    bool writeStatusLine(int status, const char* title);//写状态行
    bool writeHeader(int contentLength);//写头部字段
    bool writeBlank();//写空行
    bool writeContentLength(int contentLength);//写信息体长度
    bool writeConnection();//写连接状态
    bool writeContent(const char* content);//写信息体
    void unmap();
};

#endif

