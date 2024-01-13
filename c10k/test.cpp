#include <asm-generic/errno.h>
#include <pthread.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <csignal>
#include <thread>
#include <sys/time.h>

pthread_t *TID = NULL;
pthread_mutex_t mtx;
pthread_cond_t condt;

int f = 0;

void *test(void*)
{
    int res;
    struct timespec timeout;
    struct timeval now;
    
    // 获取当前时间
    gettimeofday(&now, nullptr);
    
    // 设置超时时间为当前时间加上5秒
    timeout.tv_sec = now.tv_sec + 5;
    timeout.tv_nsec = now.tv_usec * 1000;
    while (true) 
    {
        while ((res = pthread_cond_timedwait(&condt, &mtx, &timeout)))
        {
            // 重新设置时间
            // 获取当前时间
            gettimeofday(&now, nullptr);
            
            // 设置超时时间为当前时间加上5秒
            timeout.tv_sec = now.tv_sec + 5;
            timeout.tv_nsec = now.tv_usec * 1000;
            // 等待超时， 检测是否要关闭该线程
            pthread_t t = pthread_self();
            if (TID != NULL)
            {
                if (t == *TID) 
                {
                    puts("就是本线程");
                    f = 1;
                    pthread_exit(0);
                }
            }
            if (res == ETIMEDOUT)
            {
                puts("超时");
            }
        }
        sleep(1);
        puts("线程");
        pthread_mutex_unlock(&mtx);
    }
    pthread_exit(0);
}
int main()
{
    pthread_t tid;
    pthread_mutex_init(&mtx, NULL);
    pthread_cond_init(&condt, NULL);
    pthread_create(&tid, NULL, test, NULL);
    printf("tid = %ld\n", tid);
    int choice;
    while (true) {
        printf ("f = %d\n", f);
        if (f == 1) break;
        std::cout << "请输入： ";
        std::cin >> choice;
        if (choice == 1)
        {
            pthread_cond_signal(&condt);
        }else 
        {
            // puts("ashfg");
            TID = &tid;
        }
    }
    exit(0);
}