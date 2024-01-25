#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <sys/stat.h>
#include <fcntl.h>

#define my_size_t int

pthread_mutex_t *mptr = NULL;


int main()
{
    my_size_t socket_d, val = 1;
    struct sockaddr_in laddr, raddr;
    socklen_t len = sizeof(raddr);

    socket_d = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(8888);
    inet_pton(AF_INET, "0.0.0.0", &laddr.sin_addr.s_addr);
    if (bind(socket_d, (const struct sockaddr *)&laddr, sizeof(laddr)) < 0)
    {
        perror("bind()");
        exit(-1);
    }
    listen(socket_d, 5);

    while (true) 
    {
        my_size_t newsd = accept(socket_d, (struct sockaddr *__restrict) &raddr, &len);
        char ip[16];
        inet_ntop(AF_INET, &raddr.sin_addr.s_addr, ip, sizeof(ip));
        printf("client : %s, port = %d\n", ip, ntohs(raddr.sin_port)); 
    }
    exit(0);
}