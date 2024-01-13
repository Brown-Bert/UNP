#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#define BUFSIZE 1024

int main()
{
    char buf[BUFSIZE];
    struct sockaddr_in laddr, raddr;
    socklen_t len = sizeof(raddr);
    laddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &laddr.sin_addr.s_addr);
    laddr.sin_port = htons(8888);
    int socket_d = socket(AF_INET, SOCK_DGRAM, 0);
    if (bind(socket_d, (const struct sockaddr *)&laddr, sizeof(laddr)) < 0)
    {
        perror("bind()");
        exit(-1);
    }
    while (true) {
        int n = recvfrom(socket_d, buf, BUFSIZE, 0, (struct sockaddr *)&raddr, &len);
        char ip[16];
        inet_ntop(AF_INET, &raddr.sin_addr.s_addr, ip, sizeof(ip));
        printf("client : %s, port : %d\n", ip, ntohs(raddr.sin_port));
        sendto(socket_d, buf, n, 0, (const struct sockaddr *)&raddr, len);
    }
    exit(0);
}