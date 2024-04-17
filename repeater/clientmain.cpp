#include <unistd.h>

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

#include "repeater.h"

int main(int argc, char* argv[]) {
  // 构造1000个客户端给100个服务器发送消息
  my_int startPort = serverPortStart;
  for (int i = 0; i < 20; i++) {
    Client client(i);
    client.createSocket();
    client.setIpAndPort();
    // 创建随机数引擎
    std::random_device rd;
    std::mt19937 gen(rd());  // 使用随机设备生成种子
    // std::mt19937 gen(123); // 或者使用固定种子
    // 创建分布对象，指定随机数范围
    std::uniform_int_distribution<int> dist(10,
                                            200);  // 产生 1 到 100 之间的整数
    // 生成随机数
    int randomNum = dist(gen);
    std::string str = std::to_string(randomNum);
    if (startPort >= (serverPortStart + serverNum)) {
      startPort = serverPortStart;
    }
    client.sendMessage(serverIp, startPort, str);
    std::cout << i << std::endl;
    startPort++;
    // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    client.closefd();  // 关闭套接字描述符
  }
  return 0;
}