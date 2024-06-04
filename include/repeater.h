#ifndef REPEATER_H_
#define REPEATER_H_

#include <unistd.h>

// #include <boost/coroutine2/coroutine.hpp>
#include <mutex>
#include <string>

#include "threadPool.h"
// #include <boost/coroutine2/all.hpp>
#include <iostream>
#include <map>
#include <vector>
#include "macro.h"


int sendMessage(my_int socket_d,
                Message& mt);  // 运行客户端并发送消息，msg:具体要发送的消息

typedef struct {
  std::string desIp;  // 目的地的ip
  my_int desPort;     // 目的地的端口
  my_int socket_d;    // 中继器转发需要重新构造套接字描述符
} Info;

/**
  中继服务器
*/
extern ThreadPool* RelayThreadPool;
class RelayServer : public ServerBase {
 public:
  my_int socket_d;                    // 中继服务器的套接字描述符
  std::map<std::string, Info> infos;  // 用红黑树存储客户端与服务器配对的信息
  std::map<std::string, std::vector<std::string>>
      servers;  // 存储客户端与服务器的对应关系
  std::map<my_int, std::map<std::string, my_int>>
      fd_tasks;  // 因为多线程操作同一个描述符，会造成其他线程在处理任务的时候，有一个线程已经接收到了关闭套接字描述符的任务
                 // 为了确保关闭套接字之前，关于套接字的任务全部执行完毕，需要记录套接字相关的任务数量，以及套接字的状态：可用与不可用
  std::map<my_int, std::vector<my_int>>
      epoll_all;  // 记录每个epoll管理了哪些个描述符
  std::map<my_int, my_int> fd_epollfd;  // 记录每个描述符对应的epollfd
  my_int flaglock = true;
  std::mutex mutex_send;
  std::mutex mutex_count;
  my_int count_pkg = 0;

 public:
  RelayServer();  // 初始化中继服务器的同时初始化servers变量，并且单独开出一个线程用于输出信息
  ~RelayServer() {
    std::cout << "中继服务器析构" << std::endl;
    servers.clear();
    // delete threadPool;
  }  // 释放指向线程池的指针
  void createSocket() override;
  my_int selfCreateSocket(
      std::string ip, my_int port,
      my_int flag);  // 中继服务器创建套接字描述符, flag = 0;
                     // 表示不需要绑定本地地址，1表示需要绑定本地地址
  void recvMessage() override;  // 接收消息
  void createPool(
      my_int threadNum);  // 创建线程池, threadNum 线程池初始化时的线程个数
  void
  searchThread();  // 中继服务器上创建一个线程，用于返回给特定请求中继服务器管理的客户端与服务器的情况以及详细信息
  void recvTask(Message message, my_int fd) override;
  void myConnect(my_int fd, std::string desIp,
                 my_int desPort);  // 连接远程服务器
  void clearExit();                // 优雅退出时的清扫工作
};

/**
  @brief 服务器
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
  void createSocket() override;  // 创建客户端网络套接字描述符
  void recvTask(Message message, my_int fd) override;
  void recvMessage() override;  // 运行客户端并发送消息，msg:具体要发送的消息
  std::map<my_int, std::map<std::string, my_int>>
      fd_tasks;  // 因为多线程操作同一个描述符，会造成其他线程在处理任务的时候，有一个线程已经接收到了关闭套接字描述符的任务
  // 为了确保关闭套接字之前，关于套接字的任务全部执行完毕，需要记录套接字相关的任务数量
  void killSelf();  // 服务器自己杀死这个分离的线程
};

void searchClient();
void killThread();  // 中继器自己杀死这个分离的线程
#endif