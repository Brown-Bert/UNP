#include "socket.h"
#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <asm-generic/errno-base.h>
#include <string.h>
#include <sys/select.h>
#include <signal.h>

struct sigaction oact;

void handler(int signum)
{
    puts("信号处理函数");
    // sigaction(SIGINT, &oact, NULL);
    return;
}

int main()
{
    Client client(9527);
    client.mySocket();
    client.mySetsockopt();
    client.myBind();
    client.myConnect();
    char buf[BUFSIZE];
    char* resN;

    //注册信号行为
    struct sigaction act;
    act.sa_handler =handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    sigaction(SIGINT, &act, &oact);

    // select的使用
    int maxfd;
    fd_set rset;
    FD_ZERO(&rset);
    while (true) {
        FD_SET(fileno(stdin), &rset);
        FD_SET(client.socket_d, &rset);
        maxfd = std::max(fileno(stdin), client.socket_d) + 1;
        int res = select(maxfd, &rset, NULL, NULL, NULL);
        if (res < 0)
        {
            if (errno == EINTR) {
                puts("信号打断");
            }
            // exit(-1);
        }
        puts("continue");
        printf("select = %d\n", res);
        if (FD_ISSET(fileno(stdin), &rset))
        {
            fgets(buf, BUFSIZE, stdin);
            sendData(client.socket_d, buf, 0);
            sendData(client.socket_d, buf, 1);
        }
        if (FD_ISSET(client.socket_d, &rset))
        {
            while ((res = readData(client.socket_d, buf)) != 0)
            {
                if (!strncmp(buf, "EOF", strlen(buf))) break;
                printf("%s", buf);
            }
        }
    }

    // while ((resN = (fgets(buf, BUFSIZE, stdin))) != NULL) {
    //     // printf("fgets = %s\n", buf);
    //     sendData(client.socket_d, buf, 0);
    //     sendData(client.socket_d, buf, 1);
    //     int res = -1;
    //     // char buf1[BUFSIZE];
    //     while ((res = readData(client.socket_d, buf)) != 0)
    //     {
    //         // printf("ressdzfzd = %d\n", res);
    //         // printf("%s", buf);
    //         if (!strncmp(buf, "EOF", strlen(buf))) break;
    //         printf("%s", buf);
    //         // memset(buf, 0, BUFSIZE);
    //         // fflush(stdout);
    //     }
    //     // puts("结束");
    // }
    exit(0);
}