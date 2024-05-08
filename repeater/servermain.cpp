#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
#include <csignal>

#include "repeater.h"
#include "threadPool.h"

std::vector<Server*> serverHandles;

void signalHandler(int signal) {
    // 释放中继服务器的线程池
  for (auto it : serverHandles) it->killSelf();
    
  for (auto it : serverHandles) delete it;
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  exit(0);
}

int main(int argc, char* argv[]) {
  struct sigaction sa;
  sa.sa_handler = signalHandler; // 设置信号处理函数
  sigemptyset(&sa.sa_mask);      // 清空信号屏蔽字
  sa.sa_flags = 0;               // 设置默认标志

  // 注册信号处理程序
  if (sigaction(SIGINT, &sa, NULL) == -1) {
      std::cerr << "Failed to register signal handler" << std::endl;
      exit(-1);
  }else {
    std::cout << "信号注册成功" << std::endl;
  }
  // 创建100个服务器
  my_int startPort = serverPortStart;
  // Server server(startPort, serverIp);
  // 在一个进程中模拟多态服务器，需要把各个服务器的句柄存起来，在最后进程结束的时候释放
  for (int i = 0; i < serverNum; i++) {
    Server* server = new Server(startPort + i, serverIp);
    serverHandles.push_back(server);
    server->createSocket();
    // server.recvMessage();
    // 开启线程池
    server->threadPool = new ThreadPool(10);
    // 这种方式接收信息会造成一直卡着，因为内部实现是用while死循环，所以要把每一台服务器做成分离线程
    std::thread t(&Server::recvMessage, server);
    t.detach();  // 线程分离
    std::cout << startPort + i << std::endl;
  }
  my_int f = 0;
  std::cin >> f;
  // 释放所有服务器句柄
  for (auto it : serverHandles) it->killSelf();
    
  for (auto it : serverHandles) delete it;
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  return 0;
}