#include "Log.h"
using namespace std;


bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    this->logQueue = new BlockQueue<string>(max_queue_size);
    //创建线程异步写日志
    pthread_t id;
    pthread_create(&id, NULL, run, NULL);
    pthread_detach(id);

    //初始化
    this->lineCount = 0;
    this->closeLog = close_log;
    this->bufSize = log_buf_size;
    this->buf = new char[this->bufSize];
    memset(buf, '\0', bufSize);
    this->maxLine = split_lines;

    //获取时间
    time_t t = time(NULL);
    struct tm *sysTm = localtime(&t);
    struct tm myTm = *sysTm;

    const char* p = strrchr(file_name, '/');
    char logFullName[300] = {0};

    //日志文件名
    if(p == nullptr)
    {
        snprintf(logFullName, 299, "%d_%02d_%02d_%s", myTm.tm_year + 1900, myTm.tm_mon + 1, myTm.tm_mday, file_name);
    }
    else
    {
        strcpy(this->logName, p + 1);
        strncpy(this->dirName, file_name, p - file_name + 1);
        snprintf(logFullName, 299, "%s%d_%02d_%02d_%s", this->dirName, myTm.tm_year + 1900, myTm.tm_mon + 1, myTm.tm_mday, this->logName);
    }

    this->day = myTm.tm_mday;
    cout << 1;
    this->fp = fopen(logFullName, "a");
    if (this->fp == NULL)
    {
        return false;
    }

    return true;
}



void Log::writeLog(int level, const char *format, ...)
{
    //获取时间
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm* sysTm = localtime(&t);
    struct tm myTm = *sysTm;
    char s[16] = {0};

    //日志分级
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    
    case 1:
        strcpy(s, "[info]:");
        break;

    case 2:
        strcpy(s, "[warn]:");
        break;

    case 3:
        strcpy(s, "[error]:");
        break;

    default:
        strcpy(s, "[info]:");
        break;
    }

    this->mutex.lock();
    //更新行数
    this->lineCount++;

    if(this->day != myTm.tm_mday || this->lineCount % this->maxLine == 0)
    {
        //不是今天的日志或日志已达最大行数
        char newLog[300] = {0};
        fflush(this->fp);
        fclose(this->fp);
        char tail[16] = {0};

        //格式化
        snprintf(tail, 16, "%d_%02d_%02d_", myTm.tm_year + 1900, myTm.tm_mon + 1, myTm.tm_mday);

        if(this->day != myTm.tm_mday)
        {
            //天数错误
            //创建今天的日志 并更新天数和行数
            snprintf(newLog, 299, "%s%s%s", this->dirName, tail, this->logName);
            this->day = myTm.tm_mday;
            this->lineCount = 0;
        }
        else
        {
            //行数已满
            snprintf(newLog, 299, "%s%s%s.%lld", this->dirName, tail, this->logName, this->lineCount / this->maxLine);
        }

        this->fp = fopen(newLog, "a");
    }

    this->mutex.unlock();

    va_list valst;
    va_start(valst, format);

    string logStr;
    this->mutex.lock();

    //内容格式：时间 + 内容
    //时间格式化，snprintf成功返回写字符的总数，其中不包括结尾的null字符
    int n = snprintf(this->buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
    myTm.tm_year + 1900, myTm.tm_mon + 1, myTm.tm_mday,
    myTm.tm_hour, myTm.tm_min, myTm.tm_sec, now.tv_usec, s);

    //内容格式化
    int m = vsnprintf(this->buf + n, this->bufSize - 1, format, valst);
    this->buf[n + m] = '\n';
    this->buf[n + m + 1] = '\0';

    logStr = this->buf;

    this->mutex.unlock();

    if(!this->logQueue->full())
    {
        //队列未满
        this->logQueue->push(logStr);
    }

    va_end(valst);
}

void Log::flush()
{
    this->mutex.lock();
    fflush(this->fp);
    this->mutex.unlock();
}