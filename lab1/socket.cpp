#include "socket.h"
#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <sched.h>
#include <sys/socket.h>
#include <cstdio>
#include <cstdlib>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>

void Client::mySocket()
{
    socket_d = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_d < 0)
    {
        perror("socket()");
        exit(-1);
    }
}

void Server::mySocket()
{
    socket_d = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_d < 0)
    {
        perror("socket()");
        exit(-1);
    }
    // printf("server = %d\n", socket_d);
}

void Socket::myBind()
{
    struct sockaddr_in laddr;
    laddr.sin_family = AF_INET;
    // laddr.sin_addr.s_addr = htonl(INADDR_ANY);
    // printf("port = %d\n", port);
    inet_pton(AF_INET, "127.0.0.1", &laddr.sin_addr.s_addr);
    laddr.sin_port = htons(port);
    if (bind(socket_d, (const struct sockaddr *)&laddr, sizeof(laddr)) <0)
    {
        perror("bind()");
        exit(-1);
    }
}
void sendData(my_size_t fd, const char* strs, my_size_t flag)
{

    // 写完要发送的数据之后，还要再加上EOF作为结束，不然持续读程序会导致read阻塞, 写入一个字节的全0到管道中去
    if (flag == 1)
    {
        char end[5] = "EOF\0";
        // printf("EOF = %d\n", strlen(end));
        write(fd, end, strlen(end) + 1);
        return;
    }

    int sumBytes = strlen(strs) + 1;
    // printf("strs = %s\n", strs);
    // printf("sum = %d\n", sumBytes);
    int resN = 0;
    while (sumBytes) // 持续写
    {
        if ((resN = write(fd, strs + resN, sumBytes - resN)) < 0)
        {
            perror("write()");
            exit(-1);
        }
        // printf("resN = %d\n", resN);
        sumBytes -= resN;
        // printf("sumBytes = %d\n", sumBytes);
    }
    // puts("写入");
}
my_size_t readData(my_size_t fd, char *buf)
{
    // printf("id = %s\n", buf);
    // printf("size = %d\n", strlen(buf));
    int res = read(fd, buf, BUFSIZE);
    // printf("res = %d\n", res);
    return res;
}


Client::Client(my_size_t port)
{
    this->port = port;
}
 Client::~Client()
 {

 }

void Client::myConnect()
{
    struct sockaddr_in raddr;
    raddr.sin_family = AF_INET;
    // raddr.sin_addr.s_addr = htonl(INADDR_ANY);
    inet_pton(AF_INET, "127.0.0.1", &raddr.sin_addr.s_addr);
    raddr.sin_port = htons(8888);
    if (connect(socket_d, (const struct sockaddr *)&raddr, sizeof(raddr)) < 0)
    {
        perror("connect()");
        exit(-1);
    }
}

Server::Server(my_size_t port)
{
    this->port = port;
}
Server::~Server()
{
    close(socket_d);
}

void Socket::mySetsockopt()
{
    int val = 1;
    if (setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR,  &val, sizeof(val)) < 0)
    {
        perror("setsockopt()");
        exit(-1);
    }

    // 把发送缓冲区大小设置为0，以达到立即发送数据的目的（猜想）
    int bufsize = 0;
    if (setsockopt(socket_d, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize)) < 0)
    {
        perror("setsockopt()缓冲区");
        exit(-1);
    }
    // 把接受缓冲区设置为0
    if (setsockopt(socket_d, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)) < 0)
    {
        perror("setsockopt()缓冲区");
        exit(-1);
    }
}
void Server::myListen()
{
    if (listen(socket_d, 5) < 0)
    {
        perror("listen()");
        exit(-1);
    }
}
void Server::myAccept()
{
    int newsd;
    struct sockaddr_in raddr;
    socklen_t len = sizeof(raddr);
    pid_t pid;
    char ip[16];
    while (true) {
        newsd = accept(socket_d, (struct sockaddr *)&raddr, &len);
        if (newsd < 0)
        {
            perror("accept()");
            exit(-1);
        }
        // printf("newsd = %d\n", newsd);
        inet_ntop(AF_INET, &raddr.sin_addr.s_addr, ip, sizeof(ip));
        printf("client : %s, port : %d\n", ip, ntohs(raddr.sin_port));
        if ((pid = fork()) < 0)
        {
            perror("fork()");
            exit(-1);
        }
        if (pid == 0) // 子进程
        {
            close(socket_d);
            char buf[BUFSIZE];
            int res;
            // puts("123");
            // res = read(socket_d, buf, BUFSIZE);
            // printf("re = %d\n", res);
            while ((res = readData(newsd, buf)) != 0)
            {
                // printf("%d\n", res);
                // printf("get = %s\n", buf);
                // printf("ss\n");
                if (!strncmp(buf, "EOF", strlen(buf))) 
                {
                    sendData(newsd, buf, 1);
                    // puts("end");
                }else {
                    sendData(newsd, buf, 0);
                }
                // memset(buf, 0, sizeof(buf));
            }
            // puts("end");
            close(newsd);
            exit(0);
        }
        close(newsd);
    }
}