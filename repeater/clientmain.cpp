#include <unistd.h>

#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <csignal>

#include "repeater.h"

bool CLIENTSTOP = false;

void signalHandler(int signal) {
    // 释放中继服务器的线程池
    CLIENTSTOP = true;
}

std::string ConstructStringWithByteSize(std::size_t size) {
  std::vector<char> charVector(size, 'a');
  return std::string(charVector.begin(), charVector.end()) + '\0';
}

int main(int argc, char* argv[]) {
  // 注册信号
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
  // 构造1000个客户端给100个服务器发送消息
  if (argc < 3) {
    std::cerr << "参数有问题" << std::endl;
    return 0;
  }
  my_int startPort = serverPortStart;
  int clientNum = std::stoi(argv[1]);
  int byteSize = std::stoi(argv[2]);  // 传输的字节大小，单位值B
  // 输出
  std::cout << "clientNum: " << clientNum << std::endl;
  std::cout << "byteSize: " << byteSize << std::endl;
  std::vector<std::thread> handlers;
  for (int i = 0; i < clientNum; i++) {
    if (startPort > (serverPortStart + serverNum - 1)) {
      startPort = serverPortStart;
    }
    handlers.push_back(std::thread([=]() {
      Client client(i);
      client.createSocket();
      // sleep(2);
      client.setIpAndPort();
      std::string str = ConstructStringWithByteSize(byteSize);
      while (true) {
        if (CLIENTSTOP) break;
        client.sendMessage(serverIp, startPort, str);
        // sleep(1);
      }
      // std::cout << i << std::endl;
      // std::this_thread::sleep_for(std::chrono::milliseconds(100));
      // sleep(2);
      client.closefd();  // 关闭套接字描述符
    }));
    std::cout << "几遍 = " << handlers.size() << std::endl;
    startPort++;
  }
  for (auto& t : handlers) {
    std::cout << "join" << std::endl;
    t.join();
  }
  std::cout << "结束" << std::endl;
  return 0;
}