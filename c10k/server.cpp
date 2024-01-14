#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
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
#define TASKSQUEUESIZE 1020 // 任务队列中最多允许有1024个任务，如果超过这么多任务，那么就应该阻塞，就不能再往任务队列中存放任务

my_size_t ID = -1; // 终止线程用的

typedef struct myThread{
    my_size_t flag = -1; // -1表示此单元无效可以分配给线程，1表示此单元有效
    int state = 0; // 用来表示这个线程是否正在和客户端进行数据交换， 0表示没有进行数据交换，1表示正在进行数据交换
    pthread_t ptid; // 线程id
} myThread;

typedef struct check{
    my_size_t flag = -1; // -1表示此单元无效， 1表示此单元有效
    my_size_t count; // 统计检测低潮线程收到通知时，各个线程被调用的次数
} check;

std::queue<my_size_t> TASKSQUEUE; // 任务队列
// my_size_t TASKSQUEUE[TASKSQUEUESIZE];
my_size_t CURRENTTASKCOUNT = 0; // 用于记录目前任务个数
const my_size_t LOWLEVEL = TASKSQUEUESIZE * 20 / 100; // 任务低潮标志，当任务个数低于这个标志的时候就要减少线程数目，防止太多的线程空转，浪费系统资源
const my_size_t HIGHLEVEL = TASKSQUEUESIZE * 80 / 100; // 任务高潮标志，当任务队列中任务个数大于这个标志时，就要增加线程个数，防止任务过多把任务队列给撑爆
const my_size_t THREADLOWCOUNT = 10; // 线程低潮标志，表示至少有这个多线程在跑
const my_size_t THREADHIGHCOUNT = 100; // 表示最多有这么多线的线程在跑
const my_size_t THREADINITCOUNT = 10; // 表示服务器初始化多少个线程
my_size_t CURRENTTHREADCOUNT = 0; // 表示当前有多少个线程
const my_size_t LISTENCOUNT = 5000; // listen连接池中设置的数量

myThread THREADARRAY[THREADHIGHCOUNT]; // 线程数组
check THREADCHECK[THREADHIGHCOUNT]; // 线程检测数组，当低潮标志线程收到通知时，表明任务数量已经低于低潮标志，说明可以减少线程数量，此时根据这个
                                        // 数组可以知道此时此刻有多少个线程是没有被调用的
my_size_t FLAG = 0;
my_size_t MYFLAG = 0; // 用于同步线程

pthread_mutex_t mtx_tasks; // 修改跟tasks有关的全局数据所需要的锁
pthread_cond_t cond_notify_from_task_to_thread; // 当每一个任务进入到任务队列之后，就使用这个通知线程队列来执行
pthread_cond_t cond_notify_from_thread_to_task; // 当每一个线程执行完任务之后，需要发送通知给任务队列（有可能任务队列不需要这个通知），
                                                // 通知程序可以往任务队列中存放任务了
pthread_mutex_t mtx_thread; // 修改跟线程有关的全局数据所需要的数据
pthread_cond_t cond_notify_becauseofhigh; // 因为任务数量超过了高潮标志，此时需要通知线程去增加线程数量
pthread_cond_t cond_notify_becauseoflow; // 因为任务数量低于低潮标志，此时需要通知线程去减少线程数量
pthread_cond_t cond_tt; // 使用for循环创建线程并把i值传进线程，可能会导致线程创建太快了，导致线程内部还没有接收到传入的参数i，i就已经++了
pthread_cond_t cond_exit_thread; // 退出线程所需要使用的通知变量
pthread_mutex_t mtx_exit_thread; // 退出线程所需要的锁
pthread_mutex_t mtx_tt;
pthread_mutex_t mtx_check_tasks;
pthread_cond_t cond_check_tasks;

/**
    初始换锁
*/
void initMutex()
{
    pthread_mutex_init(&mtx_tasks, NULL);
    pthread_mutex_init(&mtx_thread, NULL);
    pthread_mutex_init(&mtx_exit_thread, NULL);
    pthread_mutex_init(&mtx_tt, NULL);
    pthread_mutex_init(&mtx_check_tasks, NULL);
    pthread_cond_init(&cond_notify_from_task_to_thread, NULL);
    pthread_cond_init(&cond_notify_from_thread_to_task, NULL);
    pthread_cond_init(&cond_notify_becauseofhigh, NULL);
    pthread_cond_init(&cond_notify_becauseoflow, NULL);
    pthread_cond_init(&cond_tt, NULL);
    pthread_cond_init(&cond_exit_thread, NULL);
    pthread_cond_init(&cond_check_tasks, NULL);
}

/**
    线程实现函数
*/
void *ThreadFunction(void* p) 
{
    int id = *(int*)(p); // 此id就是这个线程的身份，让线程自己能在THREADARRAY中能找到自己
    pthread_cond_signal(&cond_tt);

    // std::cout << "Thread " << threadNumber << " running" << std::endl;
    struct timespec timeout;
    struct timeval now;
    my_size_t res;
    
    // // 获取当前时间
    // gettimeofday(&now, nullptr);
    
    // // 设置超时时间为当前时间加上5秒
    // timeout.tv_sec = now.tv_sec + 5;
    // timeout.tv_nsec = now.tv_usec * 1000;
    // 线程执行的逻辑
    // pthread_mutex_lock(&mtx_tasks);
    while (true) 
    { // 线程一旦开启就是死循环，直到有信号来打断他结束这个线程的生命
        // printf("iiid = %d\n", id);
        // pthread_mutex_lock(&mtx_tasks);
        while (true)
        {
            // 重新设置时间
            // 获取当前时间
            // printf("res = %d\n", res);
            gettimeofday(&now, nullptr);
            
            // 设置超时时间为当前时间加上5秒
            timeout.tv_sec = now.tv_sec + 5;
            timeout.tv_nsec = now.tv_usec * 1000;
            pthread_mutex_lock(&mtx_tasks);
            res = pthread_cond_timedwait(&cond_notify_from_task_to_thread, &mtx_tasks, &timeout);
            if (res == 0)
            {
                break;
            }else if (res == ETIMEDOUT) {
                // 超时
                // 等待超时， 检测是否要关闭该线程
                // printf("超时 = %d, id = %d\n", ID, id);
                if (ID == id) 
                {
                    // 终止线程
                    printf("------------终止线程 = %d\n", id);
                    // 通知可以设置下一个ID的值了
                    pthread_mutex_lock(&mtx_exit_thread);
                    MYFLAG = 1;
                    // printf("标志位 = %d\n", MYFLAG);
                    sleep(1);
                    pthread_cond_signal(&cond_exit_thread);
                    pthread_mutex_unlock(&mtx_exit_thread);
                    pthread_mutex_unlock(&mtx_tasks);
                    pthread_exit(0);
                }
            }else {
                printf("错误");
            }
            pthread_mutex_unlock(&mtx_tasks);
        }
        // printf("id = %d\n", id);
        // while (true) 
        // {
        // 需要判断任务是否低于低潮标志，当低于低潮标志的时候需要适当的关一些线程
        // puts("抢到锁");
        if (CURRENTTASKCOUNT <= 0) 
        {
            pthread_mutex_unlock(&mtx_tasks);
            // 表明此时任务队列中没有任务，并不代表以后没有任务
            // puts("没有任务");
            continue;
        }
        CURRENTTASKCOUNT--;
        my_size_t newsd = TASKSQUEUE.front();
        TASKSQUEUE.pop();
        // printf("newsd = %d\n", newsd);
        printf("线程个数 = %d\n", CURRENTTHREADCOUNT);
        printf("任务剩余 = %d\n", CURRENTTASKCOUNT);
        pthread_mutex_unlock(&mtx_tasks);
        // printf("id = %d\n", id);
        THREADARRAY[id].state = 1;
        THREADCHECK[id].count++;
        // 从客户端接收一个数据过来，表明该处理线程需要sleep的时间，用来模拟客户端与服务器之间通信所用的时间
        char buf[BUFSIZE];
        my_size_t n = read(newsd, buf, BUFSIZE);
        std::string num(buf);
        // printf("num = %s\n", num.c_str());
        std::chrono::milliseconds dura(std::stoi(num)); // 休眠的时间是num毫秒
        // printf("dura = %d\n", dura);
        std::this_thread::sleep_for(dura);
        // puts("456");
        // sleep(2);
        THREADARRAY[id].state = 0;
        close(newsd);// 通讯完成之后关闭套接字
        // sleep(1);
        // std::this_thread::sleep_for(dura);
        pthread_cond_signal(&cond_notify_becauseoflow); // 任务减少，可能会导致低于低潮所以需要通知线程去判断
        pthread_cond_signal(&cond_notify_from_thread_to_task);
    }
    pthread_exit(0);
}

/**
    初始化线程队列
*/
void *initThreadQueue(void *)
{
    for (int i = 0; i < THREADINITCOUNT; i++) 
    {
        CURRENTTHREADCOUNT++;
        // myThread threadInstance;
        for (int j = 0; j < THREADHIGHCOUNT; j++)
        {
            if (THREADARRAY[j].flag == -1)
            {
                printf("j = %d\n", j);
                pthread_t ptid;
                if (pthread_create(&ptid, NULL, ThreadFunction, &j) < 0)
                {
                    perror("pthread_create()"); // 线程创建失败，表明系统资源不够，那么就不能再创建新的线程去执行任务，就使用原有的线程去执行任务
                    break;
                }
                pthread_detach(ptid);
                THREADARRAY[j].flag = 1;
                THREADARRAY[j].ptid = ptid;
                
                THREADCHECK[j].flag = 1;
                THREADCHECK[j].count = 0;
                break;
            }
        }
        pthread_mutex_lock(&mtx_thread);
        pthread_cond_wait(&cond_tt, &mtx_thread);
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
        pthread_mutex_lock(&mtx_thread);
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
                        printf("创建线程 = %d\n", j);
                        if (pthread_create(&ptid, NULL, ThreadFunction, &j) < 0)
                        {
                            perror("pthread_create()"); // 线程创建失败，表明系统资源不够，那么就不能再创建新的线程去执行任务，就使用原有的线程去执行任务
                            break;
                        }
                        pthread_detach(ptid);
                        THREADARRAY[j].flag = 1;
                        THREADARRAY[j].ptid = ptid;

                        THREADCHECK[j].flag = 1;
                        THREADCHECK[j].count = 0;
                        break;
                    }
                }
                pthread_mutex_lock(&mtx_tt);
                pthread_cond_wait(&cond_tt, &mtx_tt);
                pthread_mutex_unlock(&mtx_tt);
                // puts("创建完成");
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
        pthread_mutex_lock(&mtx_thread);
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
                // 先统计THREADCHECK数组中有多少个线程可以释放
                // 需要加任务锁mtx_tasks，相当于执行下面操作的时候不允许线程去执行任务，因为会修改count值，导致count统计不准确
                // pthread_mutex_lock(&mtx_tasks);
                // // 展示线程数组
                // for (int i = 0; i < THREADHIGHCOUNT; i++)
                // {
                //     if (THREADARRAY[i].flag == 1)
                //     {
                //         printf("id = %d, flag = %d, state = %d\n", i, THREADARRAY[i].flag, THREADARRAY[i].state);
                //     }
                //     if (THREADCHECK[i].flag == 1)
                //     {
                //         printf("id = %d, flag = %d, count = %d\n", i, THREADCHECK[i].flag, THREADCHECK[i].count);
                //     }
                // }
                FLAG = 1; // 断开通知
                std::this_thread::sleep_for(std::chrono::seconds(1)); // 不要立即马上统计count，因为有些线程可能还在运行
                my_size_t count = 0;
                for (int i = 0; i < THREADHIGHCOUNT; i++)
                {
                    if ((THREADCHECK[i].count == 0) && (THREADCHECK[i].flag == 1))
                    {
                        count++;
                    }
                    THREADCHECK[i].count = 0;
                }
                printf("-------------------这一轮减少的线程个数为 = %d\n", count);
                if ((CURRENTTHREADCOUNT - count) < THREADLOWCOUNT)
                {
                    // 表明此时如果减少线程count个，就会导致余下的线程个数不足线程最低标准
                    count = CURRENTTHREADCOUNT - THREADLOWCOUNT;
                }
                if (count > 10) {
                    count = 10;
                }
                // count统计出来之后，表明需要减少count个线程
                my_size_t myflag = 0;
                // sleep(5);
                // std::this_thread::sleep_for(std::chrono::seconds(5));
                for (int j = 0; j < count; j++)
                {
                    // puts("123456789");
                    printf("第%d个线程要销毁\n", j);
                    for (int i = 0; i < THREADHIGHCOUNT; i++) 
                    {
                        // puts("进入");
                        // printf("state = %d, flag = %d\n", THREADARRAY[i].state, THREADARRAY[i].flag);
                        // printf("flag = %d\n", THREADARRAY[i].flag);
                        if (THREADARRAY[i].flag == 1)
                        {
                            // printf("i = %d\n", i);
                            CURRENTTHREADCOUNT--;
                            if (myflag == 0)
                            {
                                // 第一个直接设置ID， 不用等待通知，因为第一个要作为通知这个循环的切入点
                                ID = i;
                                myflag = 1;
                            }else
                            {
                                pthread_mutex_lock(&mtx_exit_thread);
                                while (MYFLAG == 0) 
                                {
                                    pthread_cond_wait(&cond_exit_thread, &mtx_exit_thread);
                                }
                                ID = i;
                                printf("MYFLAG = %d\n", MYFLAG);
                                MYFLAG = 0;
                                pthread_mutex_unlock(&mtx_exit_thread);
                            }
                            THREADARRAY[i].flag = -1;
                            THREADCHECK[i].flag = -1;
                            break;
                        }
                    }
                }
                ID = -1;
                FLAG = 0;
                printf("剩余线程个数 = %d\n", CURRENTTHREADCOUNT);
                // pthread_mutex_unlock(&mtx_tasks);
            }
        }
        pthread_mutex_unlock(&mtx_thread);
        // puts("解锁");
    }
}

/**
    开一个线程用来检测任务队列中的任务数量，有任务就发送通知，没有任务就等待通知
*/
void *checkTasks(void*)
{
    // puts("456");
    while (true)
    {
        pthread_mutex_lock(&mtx_check_tasks);
        pthread_cond_wait(&cond_check_tasks, &mtx_check_tasks);
        // puts("789");
        while (true) {
            // puts("132");
            if (CURRENTTASKCOUNT > 0)
            {
                // printf("通知 = %d\n", CURRENTTASKCOUNT);
                if (FLAG == 0)
                {
                    pthread_cond_signal(&cond_notify_from_task_to_thread); // 任务队列中有任务，通知线程队列去执行任务
                }
            }else 
            {
                break;
            }
        }
        pthread_mutex_unlock(&mtx_check_tasks);
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
    pthread_t highId, lowId, checkId;
    pthread_create(&highId, NULL, checkHigh, NULL);
    pthread_create(&lowId, NULL, checkLow, NULL);
    pthread_create(&checkId, NULL, checkTasks, NULL);
    // 执行线程分离
    pthread_detach(highId);
    pthread_detach(lowId);
    pthread_detach(checkId);

    // pthread_cond_signal(&cond_tt);

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
        pthread_mutex_lock(&mtx_tasks); // 修改跟任务队列相关的数据的时候上锁
        // puts("抢到了");
        while (CURRENTTASKCOUNT >= TASKSQUEUESIZE)
        {
            // 表明任务数量已经超过设定的值，此时需要等待一些任务消费之后，才能再往任务队列之中添加数据
            // sleep(1);
            pthread_cond_wait(&cond_notify_from_thread_to_task, &mtx_tasks); // 为什么需要mtx_tasks这把锁，因为条件成立之后，
                                                                                         // 接下来需要往任务队列中存放任务，所以需要修改与
                                                                                         // 任务队列相关的数据
        }
        CURRENTTASKCOUNT++;
        printf("CURRENTTASKCOUNT = %d\n", CURRENTTASKCOUNT);
        TASKSQUEUE.push(newsd);
        // sleep(1);
        pthread_cond_signal(&cond_check_tasks); // 给任务监管线程发通知
        pthread_cond_signal(&cond_notify_becauseofhigh); // 此时任务数量增加了需要判断是否超过高潮标志
        pthread_mutex_unlock(&mtx_tasks); // 跟任务队列相关的数据修改完成之后解锁
    }
    exit(0);
}