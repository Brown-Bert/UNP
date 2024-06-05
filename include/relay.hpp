/**
    @file relay.h
    @brief RelayServer class header file
*/

#ifndef RELAY_H_
#define RELAY_H_

#include "base.hpp"
#include "log.hpp"
#include "macro.hpp"

/**
    @brief 中继器回话消息结构体
*/
typedef struct {
  std::string desIp;  // 目的地的ip
  my_int desPort;     // 目的地的端口
  my_int socket_d;    // 中继器转发需要重新构造套接字描述符
} Info;

/**
  @brief 中继服务器类
*/
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
  my_int count_pkg = 0;  // 用于统计中继服务器每秒平均转发报文数量

 public:
  RelayServer();  // 初始化中继服务器的同时初始化servers变量，并且单独开出一个线程用于输出信息
  ~RelayServer() {
    Logger::getInstance()->writeLogger(Logger::INFO, "中继服务器析构");
    servers.clear();
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

#endif