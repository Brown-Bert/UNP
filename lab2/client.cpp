#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdio.h>
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
    struct sockaddr_in raddr;
    raddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &raddr.sin_addr.s_addr);
    raddr.sin_port = htons(8888);

    int socket_d = socket(AF_INET, SOCK_STREAM, 0);

    int val = 1;
    setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    // setsockopt(socket_d, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));

    if (connect(socket_d, (const struct sockaddr *)&raddr, sizeof(raddr)) < 0)
    {
        perror("connect()");
        exit(-1);
    }

    char *resN = NULL;
    char buf[BUFSIZE];
    while ((resN = (fgets(buf, BUFSIZE, stdin))) != NULL) {
        sendData(socket_d, buf, 0);
        // sleep(1);
        sendData(socket_d, buf, 1);
        int res = -1;
        while ((res = readData(socket_d, buf)) != 0)
        {
            if (!strncmp(buf, "EOF", strlen(buf)))
            {
                puts("tiao");
                break;
            }
            printf("%s", buf);
        }
    }
    exit(0);
}