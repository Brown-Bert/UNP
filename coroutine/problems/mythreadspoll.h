/**
    自己写一个线程库
*/
#ifndef MYTHREADSPOLL_H_
#define MYTHREADSPOLL_H_

#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

#define my_size_t int
#define DEFAULTNUM 4
#define TASKSMAXSIZE 1020 // 一个进程所拥有的文件描述符列表（一般系统资源中默认打开的文件时1024）

class ThreadsPoll
{
private:
    // 任务队列，用于存放任务（就是套接字描述符，服务器负责接收把任务放进队列，然后通知线程中的某个线程去任务队列中拿任务执行和客户端的通信）
    std::queue<my_size_t> tasks;

    // 任务队列中最大容量
    my_size_t tasksMaxSize = TASKSMAXSIZE;

    // 多线程环境下，多个线程去操作和修改任务队列，那么任务队列就是临界资源
    std::mutex mtx;

    // 服务器要通知线程池去拿任务，就需要条件变量
    std::condition_variable cond;

    // 线程池中线程的个数
    my_size_t threadsNum = DEFAULTNUM;

    // 线程池
    std::vector<std::thread> poll;
public:
    ThreadsPoll() = default;
    ThreadsPoll(my_size_t num) {
        threadsNum = num;
    }
    void createPoll(){
        
    }
};

#endif;