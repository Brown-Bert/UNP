#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstdlib>
#include <iostream>
#include <stdio.h>

#define BUFSIZE 1024

int main()
{
    int socket_d = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in raddr;
    raddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &raddr.sin_addr.s_addr);
    raddr.sin_port = htons(8888);

    char* resN;
    char buf[BUFSIZE];
    while ((resN = (fgets(buf, BUFSIZE, stdin))) != NULL) {
        sendto(socket_d, buf, sizeof(buf), 0, (const struct sockaddr *)&raddr, sizeof(raddr));
        int n = recvfrom(socket_d, buf, BUFSIZE, 0, NULL, NULL);
        printf("%s", buf);
    }
    exit(0);
}