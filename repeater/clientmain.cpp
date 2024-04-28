#include <unistd.h>

#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "repeater.h"

std::string ConstructStringWithByteSize(std::size_t size) {
  std::vector<char> charVector(size, 'a');
  return std::string(charVector.begin(), charVector.end()) + '\0';
}

int main(int argc, char* argv[]) {
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
    handlers.push_back(std::thread([=]() {
      Client client(i);
      client.createSocket();
      // sleep(2);
      client.setIpAndPort();
      std::string str = ConstructStringWithByteSize(byteSize);
      while (true) {
        client.sendMessage(serverIp, startPort, str);
        // sleep(1);
      }
      // std::cout << i << std::endl;
      // std::this_thread::sleep_for(std::chrono::milliseconds(100));
      // sleep(2);
      client.closefd();  // 关闭套接字描述符
    }));
    std::cout << "几遍 = " << handlers.size() << std::endl;
    if (startPort > (serverPortStart + serverNum - 1)) {
      startPort = serverPortStart;
    }
    startPort++;
  }
  for (auto& t : handlers) {
    std::cout << "join" << std::endl;
    t.join();
  }
  std::cout << "结束" << std::endl;
  return 0;
}