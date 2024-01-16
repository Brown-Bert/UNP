#include <sys/types.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>

int main()
{
    pid_t pid;
    std::cout << "父进程pid = " << getpid();
    pid = fork();
    if (pid == 0)
    {
        // 子进程
        std::cout << "子进程pid = " << pid;
        exit(0);
    }
    // 父进程不收尸
    sleep(10);
    std::cout << "父进程结束";
    exit(0);
}