#include <asm-generic/socket.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <arpa/inet.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#define OPENMAX 1024
#define my_size_t int
#define BUFSIZE 1024

void sendData(my_size_t fd, const char* strs, my_size_t flag)
{

    // 写完要发送的数据之后，还要再加上EOF作为结束，不然持续读程序会导致read阻塞, 写入一个字节的全0到管道中去
    if (flag == 1)
    {
        char end[5] = "EOF\0";
        write(fd, end, strlen(end) + 1);
        return;
    }

    int sumBytes = strlen(strs) + 1;
    int resN = 0;
    while (sumBytes) // 持续写
    {
        if ((resN = write(fd, strs + resN, sumBytes - resN)) < 0)
        {
            perror("write()");
            exit(-1);
        }
        sumBytes -= resN;
    }
}
my_size_t readData(my_size_t fd, char *buf)
{
    int res = read(fd, buf, BUFSIZE);
    // 对套接字不能使用lseek
    // printf("rrrr = %s\n", buf);
    // off_t where = lseek(fd, 0, SEEK_CUR);
    // printf("当前位置 = %ld\n", where);
    // // 测试缓冲区会不会冲掉
    // where = lseek(fd, 0, SEEK_SET);
    // printf("当前位置 = %ld\n", where);
    // char b[1024];
    // read(fd, b, BUFSIZE);
    // printf("ssss = %s\n", b);
    return res;
}


int main()
{
    int socket_d;
    struct sockaddr_in laddr;

    socket_d = socket(AF_INET, SOCK_STREAM, 0);

    int val = 1;
    setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    // setsockopt(socket_d, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));

    laddr.sin_family = AF_INET;
    inet_pton(AF_INET, "0.0.0.0", &laddr.sin_addr.s_addr);
    laddr.sin_port = htons(8888);
    if (bind(socket_d, (const struct sockaddr *)&laddr, sizeof(laddr)) < 0)
    {
        perror("bind()");
        exit(-1);
    }

    listen(socket_d, 5);

    struct pollfd client[OPENMAX];
    for (int i = 0; i < OPENMAX; i++)
    {
        client[i].fd = -1;
    }

    client[0].fd = socket_d;
    client[0].events = POLLRDNORM;

    int maxi = 0;


    while (true) {
        int res = poll(client, maxi + 1, -1);
        printf("事件=%d\n", res);
        if (client[0].revents & POLLRDNORM)
        {
            // 服务器接收到了客户端的连接请求

            struct sockaddr_in raddr;
            socklen_t len = sizeof(raddr);
            int newsd = accept(client[0].fd, (struct sockaddr *)&raddr, &len);
            char ip[16];
            inet_ntop(AF_INET, &raddr.sin_addr.s_addr, ip, sizeof(ip));
            printf("client : %s, port : %d\n", ip, ntohs(raddr.sin_port));
            int i;
            for (i = 0; i < OPENMAX; i++)
            {
                if (client[i].fd < 0)
                {
                    printf("id = %d\n", newsd);
                    client[i].fd = newsd;
                    break;
                }
            }
            if (i == OPENMAX)
            {
                puts("too many clients");
                exit(-1);
            }
            client[i].events = POLLRDNORM;
            if (i > maxi) maxi = i;
            if (--res <= 0) continue;
        }
        char buf[BUFSIZ];
        for (int i = 1; i <= maxi; i++) {
            int fd = client[i].fd;
            int resN;
            if (fd < 0) continue;
            puts("1111");
            if (client[i].revents & POLLRDNORM)
            {
                puts("132");
                if ((resN = readData(fd, buf)) != 0)
                {
                    if (!strncmp(buf, "EOF", strlen(buf))) 
                    {
                        puts(buf);
                        sendData(fd, buf, 1);
                        break;
                    }else {
                        puts(buf);
                        sendData(fd, buf, 0);
                    }
                }
                if (--res <= 0)
                {
                    puts("break");
                    printf("res = %d\n", res);
                    break; // 没有读事件发生了
                }
            }
        }
        puts("789");
        // break;
    }
    exit(0);
}