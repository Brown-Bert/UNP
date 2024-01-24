#include <cstdlib>
#include <iostream>
#include "socket.h"

int main()
{
    Server server(8888);
    server.mySocket();
    server.mySetsockopt();
    server.myBind();
    server.myListen();
    server.myAccept();
    exit(0);
}