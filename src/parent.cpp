#include <unistd.h>
#include <cstdio>
#include <iostream>

int main()
{
    puts("父进程start");
    pid_t pid;
    pid = fork();
    if (pid == 0)
    {
        // 子进程
        int res = execl("./child", "./child", (char*)NULL);
        printf("res = %d\n", res);
        puts("子进程返回");
    }else {
        puts("父进程结束");
    }
    return 0;
}