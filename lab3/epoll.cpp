#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/epoll.h>


#define REVENTSSIZE 1024
#define my_size_t int

int main(int argc, char* argv[])
{
    my_size_t socket_d, value = 1;
    struct sockaddr_in laddr, raddr;

    socket_d = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_d < 0)
    {
        perror("socket()");
        // 没有成功打开套接字描述符所以不要调用close去关闭
        exit(-1);
    }

    if (setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0)
    {
        perror("setsockopt()");
        close(socket_d);
        exit(-1);
    }

    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(8888);
    inet_pton(AF_INET, "127.0.0.1", &laddr.sin_addr.s_addr);

    if (bind(socket_d, (const struct sockaddr *)&laddr, sizeof(laddr)) < 0)
    {
        perror("bind()");
        close(socket_d);
        exit(-1);
    }

    if (listen(socket_d, 5) < 0)
    {
        perror("listen()");
        close(socket_d);
        exit(-1);
    }

    // epoll
    my_size_t epollfd = epoll_create(2);
    if (epollfd < 0)
    {
        perror("epoll()");
        exit(-1);
    }

    struct epoll_event event;
    event.data.fd = socket_d;
    event.events = EPOLLIN;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, socket_d, &event);

    struct epoll_event revents[REVENTSSIZE];

    while (true)
    {
        //
        my_size_t num = epoll_wait(epollfd, revents, REVENTSSIZE, -1);
        if (num < 0)
        {
            perror("epoll_wait()");
            exit(-1);
        }
        for (my_size_t i = 0; i < num; i++)
        {
            my_size_t fd = revents[i].data.fd;
            if (fd == socket_d)
            {
                // 表明服务器接收到了来自客户端的连接请求
                socklen_t len = sizeof(raddr);
                my_size_t newsd = accept(socket_d, (struct sockaddr *__restrict)&raddr, &len);
                char ip[16];
                inet_ntop(AF_INET, &raddr.sin_addr.s_addr, ip, sizeof(ip));
                printf("client : %s, port : %d\n", ip, ntohs(raddr.sin_port));
                if (newsd < 0)
                {
                    perror("accept()");
                    close(socket_d);
                    exit(-1);
                }
                event.data.fd = newsd;
                event.events = EPOLLIN;
                epoll_ctl(epollfd, EPOLL_CTL_ADD, newsd, &event);
            }else
            {
                // 表明是已经建立的连接要通信，而不是客户端请求连接
                char buf[REVENTSSIZE];
                my_size_t len = read(fd, buf, REVENTSSIZE);
                if (len < 0)
                {
                    perror("read()");
                    close(fd);
                    close(socket_d);
                    exit(-1);
                }else if(len == 0)
                {
                    // 客户端请求关闭
                    puts("客户端请求关闭");
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                }else
                {
                    write(fd, buf, strlen(buf) + 1);
                }
            }
        }
    }
    close(socket_d);
    close(epollfd);
    exit(0);
}