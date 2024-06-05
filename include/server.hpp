/**
    @file server.h
    @brief Server class header file
*/

#ifndef SERVER_H_
#define SERVER_H_

#include "base.hpp"
#include "macro.hpp"
#include "threadPool.hpp"

/**
  @brief 服务器类
*/
class Server : public ServerBase {
 public:
  std::vector<std::string> DelayTime;  //计算消息时延
 public:
  ~Server() { delete threadPool; }
  Server(my_int port, std::string ip) {
    this->ip = ip;
    this->port = port;
  };
  void createSocket() override;  // 创建服务端网络套接字描述符
  void recvTask(Message message, my_int fd) override;
  void recvMessage() override;  // 运行服务端并发送消息
  std::map<my_int, std::map<std::string, my_int>>
      fd_tasks;  // 因为多线程操作同一个描述符，会造成其他线程在处理任务的时候，有一个线程已经接收到了关闭套接字描述符的任务
  // 为了确保关闭套接字之前，关于套接字的任务全部执行完毕，需要记录套接字相关的任务数量
  void killSelf();  // 服务器自己杀死这个分离的线程
};

#endif