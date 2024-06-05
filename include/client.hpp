/**
 * @file client.h
 * @brief Client class header file
 */

#ifndef CLIENT_H_
#define CLIENT_H_

#include "base.hpp"
#include "macro.hpp"

/**
 * @brief 客户端类
 */
class Client : public Base {
 public:
  my_int id;              // 唯一标识一个客户端
  my_int count;           // 用于每个客户端模拟通信的次数
  std::string server_ip;  // 服务器ip
  my_int server_port;     // 服务器端口
  std::string msg;        // 每个客户端自己的消息
  std::mutex
      mtx;  // 互斥锁.用于多线程发送消息的时候，确保同一时间只有一个线程使用套接字描述符
 public:
  Client(my_int id) : id(id){};
  Client(const Client& other) {
    id = other.id;
    count = other.count;
    socket_d = other.socket_d;
    ip = other.ip;
    port = other.port;
    server_ip = other.server_ip;
    server_port = other.server_port;
    msg = other.msg;
  }
  void createSocket() override;  // 创建客户端网络套接字描述符
  void
  setIpAndPort();  // 调用createSocket之后使用的是系统默认分配端口和本地ip，使用本函数获取ip和端口填入到类中
  void closefd();  // 关闭套接字描述符
  void sendMessage();  // 运行客户端并发送消息
};

#endif