#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>

int main()
{
    
    // 创建套接字
    int socket_d = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_d < 0) 
    {
        perror("socket()");
        exit(1);
    }
    
    // 绑定远程的ip和端口
    struct sockaddr_in raddr;
    raddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &raddr.sin_addr.s_addr);
    raddr.sin_port = htons(8888);
    // if (bind(socket_d, (const struct sockaddr *)&raddr, sizeof(raddr)) < 0)
    // {
    //     perror("bind()");
    //     exit(1);
    // }
    if (connect(socket_d, (const struct sockaddr *)&raddr, sizeof(raddr)) < 0)
    {
        perror("connect()");
        exit(1);
    }
    sleep(5);
    close(socket_d);
    return 0;
}