#include <csignal>
#include <iostream>
#include <thread>
#include <vector>

#include "repeater.h"
extern ThreadPool* RelayThreadPool;
extern int SIGANLSTOP;
// 中继服务器注册信号行为函数
void signalHandler(int signal) {
  // 释放中继服务器的线程池
  killThread();
  SIGANLSTOP = true;
}

int main(int argc, char* argv[]) {
  // 注册信号
  struct sigaction sa;
  sa.sa_handler = signalHandler;  // 设置信号处理函数
  sigemptyset(&sa.sa_mask);       // 清空信号屏蔽字
  sa.sa_flags = 0;                // 设置默认标志

  // 注册信号处理程序
  if (sigaction(SIGINT, &sa, NULL) == -1) {
    std::cerr << "Failed to register signal handler" << std::endl;
    exit(-1);
  } else {
    std::cout << "信号注册成功" << std::endl;
  }
  // 创建中继器
  RelayServer relayServer;
  // 创建线程池
  relayServer.createPool(100);
  relayServer.selfCreateSocket(SERVERIP, SERVERPORT, 1);
  // 多个epoll
  std::vector<std::thread> EpollHandlers;
  for (int i = 0; i < 120; i++) {
    std::thread t(&RelayServer::recvMessage, &relayServer);
    t.detach();
  }
  my_int f = 0;
  std::cin >> f;
  return 0;
}