#ifndef LOG_H
#define LOG_H


#include <iostream>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <pthread.h>
#include <cstring>
#include <ctime>
#include <sys/time.h>
#include <fstream>
#include "BlockQueue.hpp"

#define my_print3(format, ...) printf(format, ##__VA_ARGS__)

class Log
{
public:
    Log(const Log& log) = delete;
    Log operator=(const Log& log) = delete;

    static Log* getLog()
    {
        static Log log;
        return &log;
    }

    //线程执行函数
    static void* run(void* args)
    {
        Log::getLog()->writeLogToFile();
        return nullptr;
    }

    //日志文件 日志缓冲区大小 最大行数 最长日志条队列
    bool init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size);
    
    //将输出内容按标准格式整理
    void writeLog(int level, const char *format, ...);

    //强制刷新缓冲区
    void flush(void);


private:
    Log(){}
    ~Log()
    {
        if(fp != NULL)
        {
            fclose(this->fp);
        }
        delete logQueue;
    }

    void* writeLogToFile()
    {
        std::string log;
        while(logQueue->pop(log))
        {
            //取出一个日志
            mutex.lock();
            fputs(log.c_str(), this->fp);
            mutex.unlock();
        }
        return nullptr;
    }

    BlockQueue<std::string>* logQueue;//日志队列

    char dirName[128];//路径名
    char logName[128];//日志文件名
    int maxLine;//最大行数
    int bufSize;//缓冲区大小
    long long lineCount;//行数记录
    int day;//按天分类
    FILE* fp;//打开日志的文件指针
    char* buf;
    Mutex mutex;
    int closeLog;//关闭日志


};

#define LOG_DEBUG(format, ...)  Log::getLog()->writeLog(0, format, ##__VA_ARGS__); Log::getLog()->flush()
#define LOG_INFO(format, ...) Log::getLog()->writeLog(1, format, ##__VA_ARGS__); Log::getLog()->flush()
#define LOG_WARN(format, ...) Log::getLog()->writeLog(2, format, ##__VA_ARGS__); Log::getLog()->flush()
#define LOG_ERROR(format, ...) Log::getLog()->writeLog(3, format, ##__VA_ARGS__); Log::getLog()->flush()


#endif