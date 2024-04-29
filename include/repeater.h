#ifndef REPEATER_H_
#define REPEATER_H_

#include <unistd.h>

// #include <boost/coroutine2/coroutine.hpp>
#include <cstdlib>
#include <memory>
#include <string>

#include "threadPool.h"
#define my_int int
// #include <boost/coroutine2/all.hpp>
#include <iostream>
#include <map>
#include <vector>

// #include "coroutine.h"
#define SERVERIP "127.0.0.1"  // 中继器ip
#define SERVERPORT 8888       // 中继器端口
#define REVENTSSIZE 1024
#define BUFSIZE 2048          // 缓冲区的大小
#define serverPortStart 5000  // 服务器起始端口
#define serverNum 10          // 开启100台服务器
#define serverIp "127.0.0.1"  // 暂时只考虑所有服务器的ip相同
#define searchPort 9999
#define FLAG \
  true  // true : 允许服务器接收来自多个客户端的连接请求 false :
        // 不允许服务器接收多个客户端的请求

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
} Message;

/**
    客户端
*/
class Client {
 private:
  my_int id;        // 唯一标识一个客户端
  my_int count;     // 用于每个客户端模拟通信的次数
  my_int socket_d;  // 用于中继器网络套接字的描述符
  std::string ip;   // 每个客户端自己ip
  my_int port;      // 每个客户端自己端口
 public:
  Client(my_int id) : id(id){};
  void createSocket();  // 创建客户端网络套接字描述符
  void
  setIpAndPort();  // 调用createSocket之后使用的是系统默认分配端口和本地ip，使用本函数获取ip和端口填入到类中
  void sendMessage(
      std::string desIp, my_int desPort,
      std::string msg);  // 运行客户端并发送消息，msg:具体要发送的消息
  void closefd() {
    close(socket_d);  // 关闭套接字描述符
  }
};

typedef struct {
  std::string desIp;  // 目的地的ip
  my_int desPort;     // 目的地的端口
  my_int socket_d;    // 中继器转发需要重新构造套接字描述符
} Info;

/**
  中继服务器
*/
extern ThreadPool* RelayThreadPool;
class RelayServer {
 public:
  my_int socket_d;                    // 中继服务器的套接字描述符
  std::map<std::string, Info> infos;  // 用红黑树存储客户端与服务器配对的信息
  std::map<std::string, std::vector<std::string>>
      servers;             // 存储客户端与服务器的对应关系
  my_int epollfd;          // 记录epoll实例
  ThreadPool* threadPool;  // 线程池
  std::map<my_int, std::map<std::string, my_int>>
      fd_tasks;  // 因为多线程操作同一个描述符，会造成其他线程在处理任务的时候，有一个线程已经接收到了关闭套接字描述符的任务
                 // 为了确保关闭套接字之前，关于套接字的任务全部执行完毕，需要记录套接字相关的任务数量，以及套接字的状态：可用与不可用

 public:
  RelayServer();  // 初始化中继服务器的同时初始化servers变量，并且单独开出一个线程用于输出信息
  ~RelayServer() {
    servers.clear();
    delete threadPool;
  }  // 释放指向线程池的指针
  my_int createSocket(
      std::string ip, my_int port,
      my_int flag);  // 中继服务器创建套接字描述符, flag = 0;
                     // 表示不需要绑定本地地址，1表示需要绑定本地地址
  void recvMsg();  // 接收消息
  void createPool(
      my_int threadNum);  // 创建线程池, threadNum 线程池初始化时的线程个数
  void
  searchThread();  // 中继服务器上创建一个线程，用于返回给特定请求中继服务器管理的客户端与服务器的情况以及详细信息
  void coroutineFunction(char* strs, my_int fd);
  void myConnect(my_int fd, std::string desIp,
                 my_int desPort);  // 连接远程服务器
};

/**
  @brief 服务器
*/
class Server {
 private:
  std::string ip;  // 每个服务器自己ip
  my_int port;     // 每个服务器自己端口
 public:
  my_int socket_d;         // 用于中继器网络套接字的描述符
  my_int epollfd;          // 记录epoll实例
  ThreadPool* threadPool;  // 线程池
  std::map<my_int, std::vector<std::vector<std::string>>>
      MessageInfo;  // 用于存放包的信息
 public:
  ~Server() { delete threadPool; }
  Server(my_int port, std::string ip) : ip(ip), port(port){};
  void createSocket();  // 创建客户端网络套接字描述符
  void recvTask(char* strs, my_int fd);
  void recvMessage();  // 运行客户端并发送消息，msg:具体要发送的消息
  std::map<my_int, std::map<std::string, my_int>>
      fd_tasks;  // 因为多线程操作同一个描述符，会造成其他线程在处理任务的时候，有一个线程已经接收到了关闭套接字描述符的任务
  // 为了确保关闭套接字之前，关于套接字的任务全部执行完毕，需要记录套接字相关的任务数量
  void killSelf();  // 服务器自己杀死这个分离的线程
};

void searchClient();
void killThread();  // 中继器自己杀死这个分离的线程
#endif