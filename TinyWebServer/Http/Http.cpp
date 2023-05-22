#include "Http.h"
using namespace std;

const char* rootOfHtml="/home/zuizuidac1/socket/TinyWebServer/root";

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

int Http::hEpollFd = -1;
int Http::userCount = 0;
map<string, string> Http::users;
//GET /index_2013.php HTTP/1.1\r\n
//Host: www.hootina.org\r\n
//Connection: keep-alive\r\n
//Upgrade-Insecure-Requests: 1\r\n
//User-Agent: Mozilla/5.0 (Windows NT 6.1; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/65.0.3325.146 Safari/537.36\r\n
//Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8\r\n
//Accept-Encoding: gzip, deflate\r\n
//Accept-Language: zh-CN,zh;q=0.9,en;q=0.8\r\n
//\r\n

//读了内容存在data里 先判断一行一行判断是否读取完整一行 然后解析data 
//主状态最初是解析请求行状态
//在已读取的data里找\r\n 未找到\r\n 说明未获得完整的请求行 继续读取新的数据存入data
//获得完整请求行后主状态从解析请求行变为解析请求头
//请求头以\r\n 结尾 请求头每一行都以\r\n 结尾 则当遇到连续两个\r\n 说明获得完整请求头
//主状态从解析请求头变为解析请求内容


// /
// GET请求，跳转到judge.html，即欢迎访问页面
// /0
// POST请求，跳转到register.html，即注册页面
// /1
//  POST请求，跳转到log.html，即登录页面
// /2CGISQL.cgi
// POST请求，进行登录校验
// 验证成功跳转到welcome.html，即资源请求成功页面
// 验证失败跳转到logError.html，即登录失败页面
// /3CGISQL.cgi
// POST请求，进行注册校验
// 注册成功跳转到log.html，即登录页面
// 注册失败跳转到registerError.html，即注册失败页面
// /5
// POST请求，跳转到picture.html，即图片请求页面
// /6
// POST请求，跳转到video.html，即视频请求页面
// /7
// POST请求，跳转到fans.html，即关注页面

Http::Http()
{

}

Http::~Http()
{

}

void Http::modFd(const int& ev)
{
    epoll_event event;
    event.data.fd = this->clientSocket;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(Http::hEpollFd, EPOLL_CTL_MOD, this->clientSocket, &event);
}

void Http::closeConnect()
{
    if(this->clientSocket != -1)
    {
        cout << "close" << this->clientSocket << endl;
        epoll_ctl(Http::hEpollFd, EPOLL_CTL_DEL, this->clientSocket, 0);
        close(this->clientSocket);
        this->clientSocket = -1;
        Http::userCount--;
    }
}

void Http::init(int sock, struct sockaddr_in addr, char* root, string user, string passWord, string dataBaseName)
{
    this->clientSocket =  sock;
    this->clientAddress = addr;

    Http::userCount++;

    this->docRoot = root;
    strcpy(this->sqlUser, user.c_str());
    strcpy(this->sqlPassWord, passWord.c_str());
    strcpy(this->sqlName, dataBaseName.c_str());
    
    this->init();
}

void Http::init()
{
    this->mysql = nullptr;
    this->bytesToSend = 0;
    this->bytesHaveSend = 0;
    this->check = CHECK_STATE_REQUESTLINE;
    this->longConnection = false;
    this->requestMethod = 0;
    this->url = nullptr;
    this->version = nullptr;
    this->contentLength = 0;
    this->hostName = nullptr;
    this->phraseIndex = 0;
    this->checkIndex = 0;
    this->readIndex = 0;
    this->writeIndex = 0;
    this->cgi = 0;
    this->state = 0;
    this->timerFlag = 0;
    this->improv = 0;

    memset(this->data, '\0', BUFFER_MAXSIZE);
    memset(this->writeBuf, '\0', WRITE_BUFFER_SIZE);
    memset(this->file, '\0', FILENAME_LEN);
}

void Http::initMySql(ConnectionPool* contPool)
{
    MYSQL* mysql = nullptr;
    ConnectionRAII mysqlCon(&mysql, contPool);

        //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        //LOG_ERROR("Http.cpp:137 SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        Http::users[temp1] = temp2;
    }

}

bool Http::readData()
{
    //读取数据
    //ET模式
    //循环读取直至全部读完
    if(this->readIndex > BUFFER_MAXSIZE)
    {
        return false;
    }
    int dataLen = 0;
    while(true)
    {
        if((dataLen = recv(this->clientSocket, this->data + this->readIndex, BUFFER_MAXSIZE, 0)) != -1)
        {
            if(dataLen == 0)
            {
                //客户端退出
                return false;             
            }
            else
            {
                this->readIndex += dataLen;
            }
        }
        else
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return true;
            }
            std::cout << "recv error " << std::endl;
            int mErrno = errno;
            std::cout << "error: " << mErrno << std::endl;
            return false; 
        }
    }
}

void Http::phrase()
{
    //开始分析
    HTTP_CODE code = phraseRequest();
    if(code == NO_REQUEST)
    {
        //请求不完整 继续注册读事件
        //还要继续读
        modFd(EPOLLIN);
    }
    bool writeState = writeRequest(code);
    if(!writeState)
    {
        //写报文失败 断开连接
        closeConnect();
    }
    //注册写事件 将报文发送给客户端
    modFd(EPOLLOUT);
}

//分析行是否完整
Http::LINE_STATUS Http::phraseLine()
{
    //从数据中解析完整一行
    //读到完整行时将\r\n转换为\0\0
    //并让checkIndex指向下一位置
    for(; this->checkIndex < this->readIndex; this->checkIndex++)
    {
        char temp = this->data[this->checkIndex];
        if(temp == '\r')
        {
            if(this->checkIndex + 1 == this->readIndex)
            {
                //表示\r为读取数据的结尾 需继续读取数据
                return LINE_OPEN;

            }
            if(this->data[this->checkIndex + 1] == '\n')
            {
                //找到\r\n 获取完整一行
                this->data[this->checkIndex++] = '\0';
                this->data[this->checkIndex++] = '\0';
                return LINE_OK;//表示获取完整一行
            }

            return LINE_BAD;//表示请求有误
        }
        if(temp == '\n')
        {
            if(this->checkIndex > 1 && this->data[this->checkIndex - 1] == '\r')
            {
                //读取到完整行
                this->data[this->checkIndex - 1] = '\0';
                this->data[this->checkIndex++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    //循环结束仍未找到
    return LINE_OPEN;//表示需要继续读取
}

//主状态机
//返回报文解析结果
Http::HTTP_CODE Http::phraseRequest()
{
    //读请求
    //设定主状态
    //先读请求行
    //设定从状态
    LINE_STATUS line = LINE_OK;
    //设定报文解析结果
    HTTP_CODE code = NO_REQUEST;
    char* text = 0;
    while((this->check == CHECK_STATE_CONTENT && line == LINE_OK) || ((line = phraseLine()) == LINE_OK))
    {
        //不断分析是否获得完整一行
        //获得完整一行 根据当前主状态机状态选择执行哪个函数
        text = this->data + this->phraseIndex; //指向即将分析的一行的起点
        this->phraseIndex = this->checkIndex;//执行新一行的起点
        //LOG_INFO("Http.cpp:278 %s", text);
        switch (this->check)
        {
        case CHECK_STATE_REQUESTLINE:
            //调用读请求行函数
            code = readRequestLine(text);
            if(code == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            break;
        case CHECK_STATE_HEADER:
            code = readHeader(text);
            if(code == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            else if(code == GET_REQUEST)
            {
                //完整请求 调用响应函数处理
                return doRequest();
            }
            break;
        case CHECK_STATE_CONTENT:
            code = readContent(text);
            if(code == GET_REQUEST)
            {
                return doRequest();
            }
            line = LINE_OPEN;//解析完请求内容 修改状态结束循环
            break;
            
        default:
            return INTERNAL_ERROR;
        }            
    }

    //未获得完整一行
    //需要继续读取
    return NO_REQUEST;
}

Http::HTTP_CODE Http::readRequestLine(char* text)
{
    //GET /index_2013.php HTTP/1.1\r\n
    //说明请求类型、要访问的资源以及使用的HTTP版本
    //请求行以空格作为分割 不断找两者从而对请求行进行分析
    this->url = strpbrk(text, " \t");
    if(!this->url)
    {
        return BAD_REQUEST;
    }

    //将该位置改为\0，用于将前面数据取出
    *this->url++ = '\0';

    //找到第一个空格 将前面的字符串取出判断是GET还是POST
    char* method = text;
    if(strcasecmp(method, "GET") == 0)
    {
        //GET请求
        this->requestMethod = 0;

    }
    else if(strcasecmp(method, "POST") == 0)
    {
        //POST请求
        this->requestMethod = 1;
        this->cgi = 1;
    }
    else
    {
        //其他不做处理
        return BAD_REQUEST;
    }

    //继续处理
    //找到请求后第一个不是空格的字符
    this->url += strspn(this->url, " \t");
    //找到url后第一个空格字符
    this->version = strpbrk(this->url, " \t");
    if(!this->version)
    {
        return BAD_REQUEST;
    }
    *this->version++ = '\0';
    //从下一个字符开始跳过空格找到第一个非空格字符 即版本号的起点
    this->version += strspn(this->version, " \t");
    if(strcasecmp(this->version, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }

    //接下来是要访问的资源
    //url指向请求资源的起点
    //可能以https://或http://或/开头
    //如果以这两者开头 直接跳到指定的资源名称开头
    if(strncasecmp(this->url, "http://", 7) == 0)
    {
        this->url += 7;
        this->url = strchr(this->url, '/');
    }

    if(strncasecmp(this->url, "https://", 8) == 0)
    {
        this->url += 8;
        this->url = strchr(this->url, '/');
    }
    //一般直接为/或/带访问资源
    if(!this->url || this->url[0] != '/')
    {
        return BAD_REQUEST;
    }

    //资源为/时，显示欢迎界面
    if(strlen(this->url) == 1)
    {
        strcat(this->url, "judge.html");
    }

    //处理完毕 返回
    this->check = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

Http::HTTP_CODE Http::readHeader(char* text)
{
    //请求头和空行一起读
    if(text[0] == '\0')
    {
        //空行
        //GET还是POST请求
        if(this->contentLength != 0)
        {
            //POST有请求内容 跳转
            this->check = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    //请求头
    //解析请求头部连接字段
    else if(strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        //跳过空格和\t字符
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0)
        {
            //如果是长连接，则将linger标志设置为true
            this->longConnection = true;
        }
    }
    //解析请求头部内容长度字段
    else if(strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        this->contentLength = atol(text);
    }
    //解析请求头部HOST字段
    else if(strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        this->hostName = text;
    }
    else
    {
        //LOG_INFO("Http.cpp:447 oop!unknow header: %s", text);
    }
    return NO_REQUEST;

}

Http::HTTP_CODE Http::readContent(char* text)
{
    //判断是否纳入请求内容
    if(this->readIndex >= this->contentLength + this->checkIndex)
    {
        //内容为用户名密码
        text[this->contentLength] = '\0';
        this->userData = text;//保存整个信息体
        return GET_REQUEST;        
    }
    return NO_REQUEST;  

}

Http::HTTP_CODE Http::doRequest()
{
    //赋值为网站根目录
    strcpy(this->file, rootOfHtml);
    int length = strlen(rootOfHtml);

    //找到get请求中请求的url
    const char* p = strrchr(this->url, '/');

    if(this->cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {
        //3注册或者2登录
        char flag = url[1];

        char* newUrl = new char[FILENAME_LEN];
        strcpy(newUrl, "/");
        strcat(newUrl, url + 2); //字符串追加
        strncpy(this->file + length, newUrl, FILENAME_LEN - length - 1);

        //将用户名和密码提取出来
        //user=123&password=123
        char name[100], password[100];
        int i;
        for (i = 5; this->userData[i] != '&'; ++i)
        {
            name[i - 5] = userData[i];
        }
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; this->userData[i] != '\0'; ++i, ++j)
        {
            password[j] = this->userData[i];
        }
        password[j] = '\0';

        if (*(p + 1) == '3')
        {
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据
            char *sql_insert = new char[200];
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if (Http::users.find(name) == Http::users.end())
            {
                this->mutex.lock();
                int res = mysql_query(this->mysql, sql_insert);
                Http::users.insert(pair<string, string>(name, password));
                this->mutex.unlock();
                if (!res)
                {
                    strcpy(this->url, "/log.html");
                }
                else
                {
                    strcpy(this->url, "/registerError.html");
                }
            }
            else
            {
                strcpy(this->url, "/registerError.html");
            }
            delete sql_insert;
        }
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2')
        {
            if (Http::users.find(name) != Http::users.end() && Http::users[name] == password)
            {
                strcpy(this->url, "/welcome.html");
            }
            else
            {
                strcpy(this->url, "/logError.html");
            }
        }
    }


    //如果请求资源为/0，表示跳转注册界面
    if(*(p + 1) ==  '0')
    {
        char* newUrl = new char[FILENAME_LEN];
        strcpy(newUrl, "/register.html");

        //将网站目录和/register.html进行拼接，更新到m_real_file中
        strncpy(this->file + length, newUrl, strlen(newUrl));

        delete newUrl;
    }
    //登录界面
    else if(*(p + 1) == '1')
    {
        char* newUrl = new char[FILENAME_LEN];
        strcpy(newUrl, "/log.html");

        //将网站目录和/register.html进行拼接，更新到m_real_file中
        strncpy(this->file + length, newUrl, strlen(newUrl));

        delete newUrl;
    }
    else if (*(p + 1) == '5')
    {
        char* newUrl = new char[FILENAME_LEN];
        strcpy(newUrl, "/picture.html");
        strncpy(this->file + length, newUrl, strlen(newUrl));

        delete newUrl;
    }
    else if (*(p + 1) == '6')
    {
        char* newUrl = new char[FILENAME_LEN];
        strcpy(newUrl, "/video.html");
        strncpy(this->file + length, newUrl, strlen(newUrl));

        delete newUrl;
    }
    else if (*(p + 1) == '7')
    {
        char* newUrl = new char[FILENAME_LEN];
        strcpy(newUrl, "/fans.html");
        strncpy(this->file + length, newUrl, strlen(newUrl));

        delete newUrl;
    }
    //直接将url与网站目录拼接
    else
    {
        strncpy(this->file + length, url, FILENAME_LEN - length - 1);
    }


    //通过stat获取请求资源文件信息，成功则将信息更新到m_file_stat结构体
    //失败返回NO_RESOURCE状态，表示资源不存在   
    if(stat(this->file, &this->fileStat) < 0)
    {
        return NO_RESOURCE;
    }

    //判断文件的权限，是否可读，不可读则返回FORBIDDEN_REQUEST状态
    if(!(this->fileStat.st_mode & S_IROTH))
    {
        return FORBIDDEN_REQUEST;
    }

    //判断文件类型，如果是目录，则返回BAD_REQUEST，表示请求报文有误
    if(S_ISDIR(this->fileStat.st_mode))
    {
        return BAD_REQUEST;
    }

    //内存映射
    //int fd = open(this->file, O_RDONLY);
    //this->fileAddress = (char*) mmap(0, this->fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    //close(fd);

    return FILE_REQUEST;


}

bool Http::writeRequest(HTTP_CODE code)
{
    switch (code)
    {
        //内部错误 状态码500
        case INTERNAL_ERROR:
            writeStatusLine(500, error_500_title);
            writeHeader(strlen(error_500_form));
            if(!writeContent(error_500_form))
            {
                return false;
            }
            break;
    
        //报文错误 状态码404
        case BAD_REQUEST:
            writeStatusLine(404, error_404_title);
            writeHeader(strlen(error_404_form));
            if(!writeContent(error_400_form))
            {
                return false;
            }
            break;

        //资源没有访问权限    
        case FORBIDDEN_REQUEST:
            writeStatusLine(403, error_403_title);
            writeHeader(strlen(error_403_form));
            if(!writeContent(error_403_form))
            {
                return false;
            }
            break;
        
        //文件存在
        case FILE_REQUEST:
            writeStatusLine(200, ok_200_title);
            if(this->fileStat.st_size != 0)
            {
                writeHeader(this->fileStat.st_size);
                this->iv[0].iov_base = this->writeBuf;
                this->iv[0].iov_len = this->writeIndex;
                this->iv[1].iov_base = this->fileAddress;
                this->iv[1].iov_len = this->fileStat.st_size;
                this->ivCount = 2;
                this->bytesToSend = this->writeIndex + this->fileStat.st_size;
                return true;
            }
            else
            {
                const char* str = "<html><body></body></html>";
                writeHeader(strlen(str));
                if(!writeContent(str))
                {
                    return false;
                }
            }
            break;


        default:
            return false;
    }

    this->iv[0].iov_base = this->writeBuf;
    this->iv[0].iov_len = this->writeIndex;
    this->ivCount = 1;
    this->bytesToSend = this->writeIndex;
    return true;

}

bool Http::writeCommon(const char* format, ...)
{
    if(this->writeIndex >= WRITE_BUFFER_SIZE)
    {
        return false;
    }

    //可变参数列表
    va_list argList;

    va_start(argList, format);

    //将数据写入写缓冲区 返回写入长度
    int length = vsnprintf(this->writeBuf + this->writeIndex, WRITE_BUFFER_SIZE - 1 - this->writeIndex, format, argList);

    if(length >= WRITE_BUFFER_SIZE - 1 - this->writeIndex)
    {
        va_end(argList);
        return false;
    }

    this->writeIndex += length;
    va_end(argList);


    //LOG_INFO("Http.cpp:731 request:%s", this->writeBuf);
    return true;


}

//写状态行
bool Http::writeStatusLine(int status, const char* title)
{
    return this->writeCommon("%s %d %s\r\n", "HTTP/1.1", status, title);
}
//写报头
bool Http::writeHeader(int contentLength)
{
    //写文本长度         
    return writeContentLength(contentLength)
    //写连接状态 
    && writeConnection() 
    //写空行
    && writeBlank();
}

bool Http::writeContentLength(int contentLength)
{
    return writeCommon("Content-Length:%d\r\n", contentLength);
}
bool Http::writeConnection()
{
    return writeCommon("Connection:%s\r\n", (longConnection == true) ? "keep-alive" : "close");
}

bool Http::writeBlank()
{
    return writeCommon("%s", "\r\n");
}

bool Http::writeContent(const char* content)
{
    return writeCommon("%s", content);
}

void Http::unmap()
{
    if(this->fileAddress)
    {
        munmap(this->fileAddress, this->fileStat.st_size);
        this->fileAddress = nullptr;
    }
}


bool Http::response()
{
    if(this->bytesToSend == 0)
    {
        //无数据发送
        //重新注册读事件
        this->modFd(EPOLLIN);
        this->init();
        //再次初始化

        return true;

    }

    int length = 0;
    while(true)
    {
        length = writev(this->clientSocket, this->iv, this->ivCount);

        if(length < 0)
        {
            if(errno == EAGAIN)
            {
                this->modFd(EPOLLOUT);
                return true;
            }
            //连接断开
            //this->unmap();
            return false;
        }

        this->bytesHaveSend += length;
        this->bytesToSend -= length;

        if(this->bytesHaveSend >= this->iv[0].iov_len)
        {
            this->iv[0].iov_len = 0;
            this->iv[1].iov_base = this->fileAddress + (this->bytesHaveSend - this->writeIndex);
            this->iv[1].iov_len = this->bytesToSend;
        }
        else
        {
            this->iv[0].iov_base = this->writeBuf + this->bytesHaveSend;
            this->iv[0].iov_len = this->iv[0].iov_len - this->bytesHaveSend;
        }

        if(this->bytesToSend <= 0)
        {
            //this->unmap();
            this->modFd(EPOLLIN);

            if(this->longConnection)
            {
                //再次初始化
                this->init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}