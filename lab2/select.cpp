#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <system_error>
#include <string.h>
#include <unistd.h>


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
    return res;
}

int main()
{
    int socket_d;
    struct sockaddr_in laddr;

    socket_d = socket(AF_INET, SOCK_STREAM, 0);

    int val = 1;
    setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    laddr.sin_family = AF_INET;
    inet_pton(AF_INET, "0.0.0.0", &laddr.sin_addr.s_addr);
    laddr.sin_port = htons(8888);
    if (bind(socket_d, (const struct sockaddr *)&laddr, sizeof(laddr)) < 0)
    {
        perror("bind()");
        exit(-1);
    }

    listen(socket_d, 5);

    int client[FD_SETSIZE];
    fd_set rset, alrset;
    int maxfd,maxi;

    for (int i = 0; i < FD_SETSIZE; i++)
    {
        client[i] = -1;
    }

    FD_ZERO(&alrset);
    FD_SET(socket_d, &alrset);

    maxfd = socket_d;
    maxi = -1;

    while (true) {
        rset = alrset;
        int res = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (FD_ISSET(socket_d, &rset))
        {
            // 有客户端请求连接
            struct sockaddr_in raddr;
            socklen_t len = sizeof(raddr);
            int newsd = accept(socket_d, (struct sockaddr *)&raddr, &len);
            char ip[16];
            inet_ntop(AF_INET, &raddr.sin_addr.s_addr, ip, sizeof(ip));
            printf("client : %s, port : %d\n", ip, ntohs(raddr.sin_port));
            int i;
            for (i = 0; i < FD_SETSIZE; i++) {
                if (client[i] < 0)
                {
                    client[i] = newsd;
                    break;
                }
            }
            if (i == FD_SETSIZE){
                puts("too many clients");
                exit(-1);
            }
            FD_SET(newsd, &alrset);
            if (newsd > maxfd) maxfd = newsd;
            if (i > maxi) maxi = i;
            if (--res <= 0) continue;
        }
        for (int i = 0; i <= maxi; i++) {
            int fd = client[i];
            char buf[BUFSIZE];
            int resN;
            if (fd < 0) continue;
            if (FD_ISSET(fd, &rset))
            {
                if ((resN = readData(fd, buf)) != 0)
                {
                    if (!strncmp(buf, "EOF", strlen(buf))) 
                    {
                        sendData(fd, buf, 1);
                        // break;
                    }else {
                        sendData(fd, buf, 0);
                    }
                }
                if (--res <= 0) break;
            }
            // if (--res <= 0) break;
        }
    }

    exit(0);
}