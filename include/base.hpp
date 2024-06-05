/*
 * @file base.h
 * @brief 基类文件，包括最原始的基类，以及两种服务器的基类
 */

#ifndef BASE_H_
#define BASE_H_

#include <iostream>
#include <map>
#include <vector>

#include "macro.hpp"
#include "threadPool.hpp"

/**
 * @brief 客户端发送的消息结构体
 */
typedef struct {
  std::string sourceIp;  // 发送消息方的ip
  my_int sourcePort;     // 发送消息方的端口
  std::string desIp;     // 接收消息方的ip
  my_int desPort;        // 接收消息方的端口
  std::string message;   // 具体的消息
  my_int packageSize;    // 包的大小
  my_int
      packageNum;  // 包的编号，虽然tcp是先建立通道，再发送消息，底层的包是保证顺序到达，即使因为网络的原因导致包的丢失，tcp会重新发送
                   // 将分隔好的包重组再上交给上一层，但是自定义包这些是保证不了的，而且在服务器是多线程接收，更加保证不了包的顺序
  std::string timestr;  // 客户端发送消息的时间
} Message;

/**
 * @brief 所有类的基类
 */
class Base {
 public:
  std::string ip;  // 本地ip或者请求连接的ip
  my_int port;  // 本地绑定之后自动分配的端口或者请求连接的端口
  my_int
      socket_d;  // 本地请求连接的套接字描述符或者服务器被请求连接之后自动分配的套接字描述符

 public:
  virtual void createSocket() = 0;  // 创建网络套接字描述符
};

/**
 * @brief 服务器的基类(包括中继服务器和真正接收消息的服务器)
 */
class ServerBase : public Base {
 public:
  my_int epollfd;                    // 记录epoll实例
  ThreadPool* threadPool = nullptr;  // 线程池
  std::map<my_int, std::vector<std::vector<std::string>>>
      MessageInfo;  // 用于存放包的信息，中继服务器也需要组包，不然多线程环境下不能保证包到达对面的顺序，在多线程乱序发送的环境下才需要手动组包
 public:
  virtual void recvTask(Message message,
                        my_int fd) = 0;  // 线程池任务队列中存放的任务
  virtual void recvMessage() = 0;  // 接收消息（基于此开启线程）
};

#endif