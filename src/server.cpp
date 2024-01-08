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
    struct sockaddr_in laddr, raddr;
    socklen_t len = sizeof(raddr);
    laddr.sin_family = AF_INET;
    int newsd;
    pid_t pid;
    inet_pton(AF_INET, "127.0.0.1", &laddr.sin_addr.s_addr);
    laddr.sin_port = htons(8888);
    if (bind(socket_d, (const struct sockaddr *)&laddr, sizeof(laddr)) < 0)
    {
        perror("bind()");
        exit(1);
    }
    if (listen(socket_d, 5) < 0)
    {
        perror("listen()");
        exit(1);
    }
    while (true) {
        newsd = accept(socket_d, (struct sockaddr *)&raddr, &len);
        if (newsd < 0)
        {
            perror("accept");
            exit(1);
        }
        char ip[16];
        inet_ntop(AF_INET, &raddr.sin_addr, ip, sizeof(ip));
        printf("client : %s, port : %d\n", ip, ntohs(raddr.sin_port));
        // 创建子进程
        pid = fork();
        if (pid ==0 ) // 子进程
        {
            // 做相应的处理
            close(newsd);
        }
    }
    close(socket_d);
    return 0;
}