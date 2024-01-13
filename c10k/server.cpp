#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <queue>
#include <map>
#include <functional>
#include <string>
#include <thread>
#include <sys/time.h>


#define my_size_t int
#define BUFSIZE 1024
#define TASKSQUEUESIZE 1024 // 任务队列中最多允许有1024个任务，如果超过这么多任务，那么就应该阻塞，就不能再往任务队列中存放任务

my_size_t ID = -1; // 终止线程用的

typedef struct myThread{
    my_size_t flag = -1; // -1表示此单元无效可以分配给线程，1表示此单元有效
    int state = 0; // 用来表示这个线程是否正在和客户端进行数据交换， 0表示没有进行数据交换，1表示正在进行数据交换
    pthread_t ptid; // 线程id
} myThread;

std::queue<my_size_t> TASKSQUEUE; // 任务队列
// my_size_t TASKSQUEUE[TASKSQUEUESIZE];
my_size_t CURRENTTASKCOUNT = 0; // 用于记录目前任务个数
const my_size_t LOWLEVEL = 100; // 任务低潮标志，当任务个数低于这个标志的时候就要减少线程数目，防止太多的线程空转，浪费系统资源
const my_size_t HIGHLEVEL = 800; // 任务高潮标志，当任务队列中任务个数大于这个标志时，就要增加线程个数，防止任务过多把任务队列给撑爆
const my_size_t THREADLOWCOUNT = 10; // 线程低潮标志，表示至少有这个多线程在跑
const my_size_t THREADHIGHCOUNT = 100; // 表示最多有这么多线的线程在跑
const my_size_t THREADINITCOUNT = 10; // 表示服务器初始化多少个线程
my_size_t CURRENTTHREADCOUNT = 0; // 表示当前有多少个线程
const my_size_t LISTENCOUNT = 5000; // listen连接池中设置的数量

myThread THREADARRAY[THREADHIGHCOUNT]; // 线程数组


pthread_mutex_t mtx_tasks; // 修改跟tasks有关的全局数据所需要的锁
pthread_cond_t cond_notify_from_task_to_thread; // 当每一个任务进入到任务队列之后，就使用这个通知线程队列来执行
pthread_cond_t cond_notify_from_thread_to_task; // 当每一个线程执行完任务之后，需要发送通知给任务队列（有可能任务队列不需要这个通知），
                                                // 通知程序可以往任务队列中存放任务了
pthread_mutex_t mtx_thread; // 修改跟线程有关的全局数据所需要的数据
pthread_cond_t cond_notify_becauseofhigh; // 因为任务数量超过了高潮标志，此时需要通知线程去增加线程数量
pthread_cond_t cond_notify_becauseoflow; // 因为任务数量低于低潮标志，此时需要通知线程去减少线程数量
pthread_cond_t cond_tt; // 使用for循环创建线程并把i值传进线程，可能会导致线程创建太快了，导致线程内部还没有接收到传入的参数i，i就已经++了


/**
    初始换锁
*/
void initMutex()
{
    pthread_mutex_init(&mtx_tasks, NULL);
    pthread_mutex_init(&mtx_thread, NULL);
    pthread_cond_init(&cond_notify_from_task_to_thread, NULL);
    pthread_cond_init(&cond_notify_from_thread_to_task, NULL);
    pthread_cond_init(&cond_notify_becauseofhigh, NULL);
    pthread_cond_init(&cond_notify_becauseoflow, NULL);
    pthread_cond_init(&cond_tt, NULL);
}

/**
    线程实现函数
*/
void *ThreadFunction(void* p) 
{
    int id = *(int*)(p); // 此id就是这个线程的身份，让线程自己能在THREADARRAY中能找到自己
    // printf("iiid = %d\n", id);
    pthread_cond_signal(&cond_tt);

    // std::cout << "Thread " << threadNumber << " running" << std::endl;
    struct timespec timeout;
    struct timeval now;
    my_size_t res;
    
    // 获取当前时间
    gettimeofday(&now, nullptr);
    
    // 设置超时时间为当前时间加上5秒
    timeout.tv_sec = now.tv_sec + 5;
    timeout.tv_nsec = now.tv_usec * 1000;
    // 线程执行的逻辑
    while (true) 
    { // 线程一旦开启就是死循环，直到有信号来打断他结束这个线程的生命
        // pthread_cond_wait(&cond_notify_from_task_to_thread, &mtx_tasks);
        while ((res = pthread_cond_timedwait(&cond_notify_from_task_to_thread, &mtx_tasks, &timeout)))
        {
            // 重新设置时间
            // 获取当前时间
            gettimeofday(&now, nullptr);
            
            // 设置超时时间为当前时间加上5秒
            timeout.tv_sec = now.tv_sec + 5;
            timeout.tv_nsec = now.tv_usec * 1000;
            // 等待超时， 检测是否要关闭该线程
            if (ID != -1)
            {
                if (ID == id) 
                {
                    // 终止线程
                    pthread_exit(0);
                }
            }
        }
        while (true) 
        {
            CURRENTTASKCOUNT--;
            my_size_t newsd = TASKSQUEUE.front();
            TASKSQUEUE.pop();
            pthread_cond_signal(&cond_notify_becauseoflow); // 任务减少，可能会导致低于低潮所以需要通知线程去判断
            // printf("newsd = %d\n", newsd);
            pthread_mutex_unlock(&mtx_tasks);
            // printf("id = %d\n", id);
            THREADARRAY[id].state = 1;
            // 从客户端接收一个数据过来，表明该处理线程需要sleep的时间，用来模拟客户端与服务器之间通信所用的时间
            char buf[BUFSIZE];
            my_size_t n = read(newsd, buf, BUFSIZE);
            std::string num(buf);
            // printf("num = %s\n", num.c_str());
            std::chrono::microseconds dura(std::stoi(num)); // 休眠的时间是num毫秒
            // printf("dura = %d\n", dura);
            // std::this_thread::sleep_for(dura);
            // puts("456");
            sleep(2);
            // 需要判断任务是否低于低潮标志，当低于低潮标志的时候需要适当的关一些线程
             printf("id = %d\n", id);
            if (CURRENTTASKCOUNT == 0)
            {
                // 表明此时没有任务，则需要突出循环
                puts("没有任务");
                break;
            }
            THREADARRAY[id].state = 0;
            pthread_mutex_lock(&mtx_tasks);
        }
    }
}

/**
    初始化线程队列
*/
void *initThreadQueue(void *)
{
    for (int i = 0; i < THREADINITCOUNT; i++) 
    {
        pthread_cond_wait(&cond_tt, &mtx_thread);
        CURRENTTHREADCOUNT++;
        // myThread threadInstance;
        pthread_t ptid;
        for (int j = 0; j < THREADHIGHCOUNT; j++)
        {
            if (THREADARRAY[j].flag == -1)
            {
                printf("j = %d\n", j);
                if (pthread_create(&ptid, NULL, ThreadFunction, &j) < 0)
                {
                    perror("pthread_create()"); // 线程创建失败，表明系统资源不够，那么就不能再创建新的线程去执行任务，就使用原有的线程去执行任务
                    break;
                }
                pthread_detach(ptid);
                THREADARRAY[j].flag = 1;
                THREADARRAY[j].ptid = ptid;
                break;
            }
        }
        // sleep(1);
        // 要获取到

        pthread_mutex_unlock(&mtx_thread); // 其实在此处可以不需要锁，因为此时是初始化，根本不会出现资源竞争的现象
    }
    pthread_exit(0);
}

/**
    创建一个检测线程，用于检测系统资源的情况，检测任务数量是否超过高潮标志
*/

void *checkHigh(void*)
{
    while (true)
    {
        pthread_cond_wait(&cond_notify_becauseofhigh, &mtx_thread);
        if (CURRENTTASKCOUNT > HIGHLEVEL)
        {
            // 此时任务达到了高潮标志，需要增加线程的数量来快速消费任务
            // 增加线程前需要判断系统资源是否够
            if (CURRENTTHREADCOUNT >= THREADHIGHCOUNT)
            {
                // 表明系统资源不允许再创建线程去消费任务了
            }else 
            {
                // 表明此时系统还可以创建线程去消费任务
                // pthread_mutex_lock(&mtx_thread);
                CURRENTTHREADCOUNT++;
                pthread_t ptid;
                for (int j = 0; j < THREADHIGHCOUNT; j++)
                {
                    if (THREADARRAY[j].flag == -1)
                    {
                        if (pthread_create(&ptid, NULL, ThreadFunction, &j) < 0)
                        {
                            perror("pthread_create()"); // 线程创建失败，表明系统资源不够，那么就不能再创建新的线程去执行任务，就使用原有的线程去执行任务
                            break;
                        }
                        pthread_detach(ptid);
                        THREADARRAY[j].flag = 1;
                        THREADARRAY[j].ptid = ptid;
                        break;
                    }
                } 
            }
        }
        pthread_mutex_unlock(&mtx_thread); // 其实在此处可以不需要锁，因为此时是初始化，根本不会出现资源竞争的现象
    }
}

/**
    创建一个检测线程，用于检测系统资源的情况，检测任务数量是否低于低潮标志
*/

void *checkLow(void*)
{
    while (true)
    {
        pthread_cond_wait(&cond_notify_becauseoflow, &mtx_thread);
        if (CURRENTTASKCOUNT < LOWLEVEL)
        {
            // 此时任务达到了低潮标志，需要减少线程数量
            // 如果此时线程的数量就是低潮标志，那么就不要去减少线程数量
            if (CURRENTTHREADCOUNT <= THREADLOWCOUNT)
            {
                // 表明系统资源不需要再减少线程去释放系统资源
            }else 
            {
                // 表明此时系统需要去减少线程释放资源
                // pthread_mutex_lock(&mtx_thread);
                CURRENTTHREADCOUNT--;
                for (int i = 0; i < THREADHIGHCOUNT; i++) 
                {
                    if (THREADARRAY[i].state == 0)
                    {
                        ID = i;
                        THREADARRAY->flag = -1;
                    }
                }
            }
        }
        pthread_mutex_unlock(&mtx_thread); // 其实在此处可以不需要锁，因为此时是初始化，根本不会出现资源竞争的现象
    }
}


int main(int argc, char* argv[])
{
    pthread_t init_tid;

    // 锁的初始化
    initMutex();

    // 线程队列的初始化
    // initThreadQueue();
    pthread_create(&init_tid, NULL, initThreadQueue, NULL);
    pthread_detach(init_tid);

    // printf("CURRENTTHREADCOUNT = %d\n", CURRENTTHREADCOUNT);

    // 开启检测的线程
    pthread_t highId, lowId;
    pthread_create(&highId, NULL, checkHigh, NULL);
    pthread_create(&lowId, NULL, checkLow, NULL);
    // 执行线程分离
    pthread_detach(highId);
    pthread_detach(lowId);

    pthread_cond_signal(&cond_tt);

    my_size_t socket_d, val = 1;
    struct sockaddr_in laddr, raddr;
    socklen_t len = sizeof(raddr);

    socket_d = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(8888);
    inet_pton(AF_INET, "0.0.0.0", &laddr.sin_addr.s_addr);
    if (bind(socket_d, (const struct sockaddr *)&laddr, sizeof(laddr)) < 0)
    {
        perror("bind()");
        exit(-1);
    }
    listen(socket_d, LISTENCOUNT);
    while (true) 
    {
        // 建立连接
        my_size_t newsd = accept(socket_d, (struct sockaddr *__restrict) &raddr, &len);
        char ip[16];
        inet_ntop(AF_INET, &raddr.sin_addr.s_addr, ip, sizeof(ip));
        printf("client : %s, port = %d\n", ip, ntohs(raddr.sin_port));
        if (CURRENTTASKCOUNT < TASKSQUEUESIZE) 
        {
            // 任务队列还没有满，可以继续存放任务
            pthread_mutex_lock(&mtx_tasks); // 修改跟任务队列相关的数据的时候上锁
        }else 
        {
            // 表明任务数量已经超过设定的值，此时需要等待一些任务消费之后，才能再往任务队列之中添加数据
            pthread_cond_wait(&cond_notify_from_thread_to_task, &mtx_tasks); // 为什么需要mtx_tasks这把锁，因为条件成立之后，
                                                                                         // 接下来需要往任务队列中存放任务，所以需要修改与
                                                                                         // 任务队列相关的数据
        }
        CURRENTTASKCOUNT++;
        printf("CURRENTTASKCOUNT = %d\n", CURRENTTASKCOUNT);
        TASKSQUEUE.push(newsd);
        pthread_cond_signal(&cond_notify_from_task_to_thread); // 有一个任务进入任务队列，通知线程队列去执行任务
        pthread_cond_signal(&cond_notify_becauseofhigh); // 此时任务数量增加了需要判断是否超过高潮标志
        pthread_mutex_unlock(&mtx_tasks); // 跟任务队列相关的数据修改完成之后解锁
    }
    exit(0);
}