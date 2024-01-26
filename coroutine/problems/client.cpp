#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>

void send(int flag) {  // flag=1表示开始计时，0表示结束计时，其他表示进行通信
  int socket_d;
  sockaddr_in raddr;
  raddr.sin_family = AF_INET;
  raddr.sin_port = htons(8888);
  inet_pton(AF_INET, "127.0.0.1", &raddr.sin_addr.s_addr);
  socket_d = socket(AF_INET, SOCK_STREAM, 0);
  int value = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
  if (connect(socket_d, (const struct sockaddr*)&raddr, sizeof(raddr)) < 0) {
    perror("connect()");
    exit(-1);
  }
  if (flag == 1) {
    char b[2] = {'1', '\0'};
    write(socket_d, b, sizeof(b));
  } else if (flag == 0) {
    char b[2] = {'0', '\0'};
    write(socket_d, b, sizeof(b));
  } else {
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
    write(socket_d, str.c_str(), sizeof(str));
  }
  close(socket_d);
}

int main(int argc, char* argv[]) {
  for (int j = 0; j < 10; j++) {  // 测10轮时间
    send(1);
    for (int i = 0; i < 100; i++)  // 创建10个客户端去连接服务器
    {
      send(3);
    }
    send(0);
  }
  exit(0);
}