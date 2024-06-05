#include <csignal>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "macro.hpp"
#include "relay.hpp"
#include "utils.hpp"
extern ThreadPool* RelayThreadPool;
extern int SIGANLSTOP;
std::ofstream logFileObj("./log/logger.txt");
Logger* Logger::instance = nullptr;
// 中继服务器注册信号行为函数
void signalHandler(my_int signal) {
  // 释放中继服务器的线程池
  killThread();
  SIGANLSTOP = true;
}

my_int main(my_int argc, char* argv[]) {
  if (argc < 3) {
    std::cerr << "./relaymain <epoll实例个数> <线程池中线程个数>" << std::endl;
    Logger::getInstance()->writeLogger(Logger::ERROR,
                                       "参数有问题 in clientmain");
    return 0;
  }

  my_int relay_server_num = std::stoi(argv[1]);  // 开启多少个epoll实例
  my_int relay_server_poll_num = std::stoi(argv[2]);  // 所有epoll实例共享线程池

  // 注册信号
  struct sigaction sa;
  sa.sa_handler = signalHandler;  // 设置信号处理函数
  sigemptyset(&sa.sa_mask);       // 清空信号屏蔽字
  sa.sa_flags = 0;                // 设置默认标志

  // 注册信号处理程序
  if (sigaction(SIGINT, &sa, NULL) == -1) {
    std::cerr << "Failed to register signal handler" << std::endl;
    Logger::getInstance()->writeLogger(
        Logger::ERROR, "Failed to register signal handler in relaymain");
    exit(-1);
  } else {
    std::cout << "信号注册成功" << std::endl;
    Logger::getInstance()->writeLogger(Logger::INFO,
                                       "信号注册成功 in relaymain");
  }
  // 创建中继器
  RelayServer relayServer;
  // 创建线程池
  relayServer.createPool(relay_server_poll_num);
  relayServer.selfCreateSocket(SERVERIP, SERVERPORT, 1);
  // 多个epoll
  std::vector<std::thread> EpollHandlers;
  for (int i = 0; i < relay_server_num; i++) {
    std::thread t(&RelayServer::recvMessage, &relayServer);
    t.detach();
  }
  my_int f = 0;
  std::cin >> f;
  return 0;
}