#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#include "repeater.h"
#include "threadPool.h"

int main(int argc, char* argv[]) {
  // 创建100个服务器
  my_int startPort = serverPortStart;
  // Server server(startPort, serverIp);
  // 在一个进程中模拟多态服务器，需要把各个服务器的句柄存起来，在最后进程结束的时候释放
  std::vector<Server*> serverHandles;
  for (int i = 0; i < serverNum; i++) {
    Server* server = new Server(startPort + i, serverIp);
    serverHandles.push_back(server);
    server->createSocket();
    // server.recvMessage();
    // 开启线程池
    server->threadPool = new ThreadPool(50);
    // 这种方式接收信息会造成一直卡着，因为内部实现是用while死循环，所以要把每一台服务器做成分离线程
    std::thread t(&Server::recvMessage, server);
    t.detach();  // 线程分离
    std::cout << startPort + i << std::endl;
  }
  my_int f = 0;
  std::cin >> f;
  // 释放所有服务器句柄
  for (auto it : serverHandles) {
    free(it);
  }
  return 0;
}