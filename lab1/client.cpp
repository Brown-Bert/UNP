#include "socket.h"
#include <cstdio>
#include <iostream>
#include <string.h>

int main()
{
    Client client(9527);
    client.mySocket();
    client.mySetsockopt();
    client.myBind();
    client.myConnect();
    char buf[BUFSIZE];
    char* resN;
    while ((resN = (fgets(buf, BUFSIZE, stdin))) != NULL) {
        // printf("fgets = %s\n", buf);
        sendData(client.socket_d, buf, 0);
        sendData(client.socket_d, buf, 1);
        int res = -1;
        // char buf1[BUFSIZE];
        while ((res = readData(client.socket_d, buf)) != 0)
        {
            // printf("ressdzfzd = %d\n", res);
            // printf("%s", buf);
            if (!strncmp(buf, "EOF", strlen(buf))) break;
            printf("%s", buf);
            // memset(buf, 0, BUFSIZE);
            // fflush(stdout);
        }
        // puts("结束");
    }
    exit(0);
}