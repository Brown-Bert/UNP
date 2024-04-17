#include "repeater.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "threadPool.h"

/**
  std::string sourceIp;  // 发送消息方的ip
  my_int sourcePort;     // 发送消息方的端口
  std::string desIp;     // 接收消息方的ip
  my_int desPort;        // 接收消息方的端口
  std::string message;   // 具体的消息
*/

// 序列化结构体为字节流
void serializeStruct(const Message &message, char *buffer) {
  // 序列化发送方ip字符串成员的长度和字符数据
  int messageLength = message.sourceIp.length();
  memcpy(buffer, &messageLength, sizeof(int));
  buffer += sizeof(int);
  memcpy(buffer, message.sourceIp.c_str(), messageLength);
  buffer += messageLength;

  // 序列化发送方port
  memcpy(buffer, &message.sourcePort, sizeof(int));
  buffer += sizeof(int);

  // 序列化接收方ip字符串成员的长度和字符数据
  messageLength = message.desIp.length();
  memcpy(buffer, &messageLength, sizeof(int));
  buffer += sizeof(int);
  memcpy(buffer, message.desIp.c_str(), messageLength);
  buffer += messageLength;

  // 序列化发送方port
  memcpy(buffer, &message.desPort, sizeof(int));
  buffer += sizeof(int);

  // 序列化具体的消息
  messageLength = message.message.length();
  memcpy(buffer, &messageLength, sizeof(int));
  buffer += sizeof(int);
  memcpy(buffer, message.message.c_str(), messageLength);
}

// 反序列化字节流为结构体
void deserializeStruct(const char *buffer, Message &message) {
  // 反序列化发送方字符串成员的长度和字符数据
  int messageLength;
  memcpy(&messageLength, buffer, sizeof(int));
  buffer += sizeof(int);
  message.sourceIp.assign(buffer, messageLength);
  buffer += messageLength;

  // 反序列化发送方的port
  memcpy(&message.sourcePort, buffer, sizeof(int));
  buffer += sizeof(int);

  // 反序列化接收方字符串成员的长度和字符数据
  memcpy(&messageLength, buffer, sizeof(int));
  buffer += sizeof(int);
  message.desIp.assign(buffer, messageLength);
  buffer += messageLength;

  // 反序列化接收方的port
  memcpy(&message.desPort, buffer, sizeof(int));
  buffer += sizeof(int);

  // 反序列化具体消息
  memcpy(&messageLength, buffer, sizeof(int));
  buffer += sizeof(int);
  message.message.assign(buffer, messageLength);
}

void Client::createSocket() {
  struct sockaddr_in raddr;
  raddr.sin_family = AF_INET;
  inet_pton(AF_INET, SERVERIP, &raddr.sin_addr.s_addr);
  raddr.sin_port = htons(SERVERPORT);

  socket_d = socket(AF_INET, SOCK_STREAM, 0);

  int val = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  // setsockopt(socket_d, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));

  if (connect(socket_d, (const struct sockaddr *)&raddr, sizeof(raddr)) < 0) {
    perror("connect()");
    exit(-1);
  }
}

void Client::setIpAndPort() {
  struct sockaddr_in laddr;
  socklen_t len = sizeof(laddr);
  if (getsockname(socket_d, (struct sockaddr *)&laddr, &len) < 0) {
    perror("getsockname()");
    exit(-1);
  }
  char ipt[16];
  inet_ntop(AF_INET, &laddr.sin_addr.s_addr, ipt, sizeof(ipt));
  ip = ipt;
  port = laddr.sin_port;
}

void Client::sendMessage(std::string desIp, my_int desPort, std::string msg) {
  // 发送信息
  Message message;
  message.sourceIp = ip;
  message.sourcePort = port;
  message.desIp = desIp;
  message.desPort = desPort;
  message.message = msg;
  // 如果消息够长，则会造成缓冲区溢出，而且自己定义的数据结构在接收方并不能按照预计的那样去读取，所以需要将具体的消息手动切片或者补齐
  // 将结构体序列化
  char buf[BUFSIZE];
  // memcpy(buf, &message, sizeof(message));
  serializeStruct(message, buf);
  write(socket_d, buf, sizeof(buf));
}

void RelayServer::searchThread() {
  struct sockaddr_in laddr, raddr;
  laddr.sin_family = AF_INET;
  inet_pton(AF_INET, serverIp, &laddr.sin_addr.s_addr);
  laddr.sin_port = htons(searchPort);

  my_int socket_d = socket(AF_INET, SOCK_STREAM, 0);

  int val = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  // setsockopt(socket_d, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));

  if (bind(socket_d, (const struct sockaddr *)&laddr, sizeof(laddr)) < 0) {
    perror("bind");
    exit(-1);
  }
  if (listen(socket_d, 50) < 0) {
    perror("listen()");
    exit(-1);
  }
  socklen_t len;
  my_int newsd = accept(socket_d, (struct sockaddr *__restrict)&raddr, &len);
  char ip[16];
  inet_ntop(AF_INET, &raddr.sin_addr.s_addr, ip, sizeof(ip));
  printf("client : %s, port = %d\n", ip, ntohs(raddr.sin_port));
  char buf[BUFSIZE];
  while (true) {
    read(newsd, buf, BUFSIZE);
    // 返回信息
    // 将 map 序列化为字节流
    std::string serializedMap;
    for (const auto &pair : servers) {
      // 序列化键
      serializedMap += pair.first + ":";

      // 序列化向量长度
      int vectorSize = pair.second.size();
      serializedMap += std::to_string(vectorSize) + ",";

      // 序列化向量元素
      for (const auto &element : pair.second) {
        serializedMap += element + ",";
      }

      serializedMap += ";";
    }
    // 返回数据给请求方
    if (send(newsd, serializedMap.c_str(), serializedMap.size(), 0) == -1) {
      perror("send()");
      exit(-1);
    }
  }
}

RelayServer::RelayServer() {
  // 初始化servers
  std::vector<std::string> v;
  for (int i = 0; i < serverNum; i++) {
    servers.insert(std::pair<std::string, std::vector<std::string>>(
        std::to_string(serverPortStart + i), v));
  }
  // 开启线程
  std::thread t(&RelayServer::searchThread, this);
  t.detach();  // 线程分离
}

my_int RelayServer::createSocket(std::string ip, my_int port, my_int flag) {
  struct sockaddr_in laddr;
  laddr.sin_family = AF_INET;
  inet_pton(AF_INET, ip.c_str(), &laddr.sin_addr.s_addr);
  laddr.sin_port = htons(port);

  my_int fd = socket(AF_INET, SOCK_STREAM, 0);

  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  // setsockopt(socket_d, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
  if (flag) {
    socket_d = fd;
    if (bind(fd, (const struct sockaddr *)&laddr, sizeof(laddr)) < 0) {
      perror("bind()");
      exit(-1);
    }
  }
  return fd;
}

void RelayServer::createPool(my_int threadNum) {
  threadPool = new ThreadPool(threadNum);
}

void RelayServer::myConnect(my_int fd, std::string desIp, my_int desPort) {
  struct sockaddr_in raddr;
  raddr.sin_family = AF_INET;
  inet_pton(AF_INET, desIp.c_str(), &raddr.sin_addr.s_addr);
  raddr.sin_port = htons(desPort);
  // setsockopt(socket_d, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));

  if (connect(fd, (const struct sockaddr *)&raddr, sizeof(raddr)) < 0) {
    perror("connect()");
    exit(-1);
  }
}

std::mutex mutex_infos;
std::mutex mutex_epollfd;
std::mutex mutex_task;
std::mutex mutex_server;

void RelayServer::coroutineFunction(char *strs, my_int len, my_int fd) {
  // while (true) {
  std::cout << "len = " << len << std::endl;
  char buf[BUFSIZE];
  memcpy(buf, strs, BUFSIZE);
  if (len == 0) {
    // std::cout << "errno = " << errno << std::endl;
    // 客户端请求关闭，关闭客户端的套接字描述符，同时也要向服务器请求关闭，还要关闭停留在中继服务器的红黑树中的结构体信息
    // 如果与fd相关的任务还没有执行完成，则不能进行关闭套接字描述符的操作
    // 记录套接字相关的任务数
    // 先查询fd_tasks中是不是已经存在fd
    auto it = fd_tasks.find(fd);
    // std::cout << "空转/////" << it->second << std::endl;
    while (it->second["number"] != 0) {
      // 表明还有任务没有执行完成不能执行关闭操作
      std::cout << "空转" << it->second["number"] << std::endl;
      sleep(5);  // 暂时让其5s空转一次
    }
    {
      std::unique_lock<std::mutex> lock(mutex_task);
      // 修改fd_tasks中的记录
      it->second["number"] = -1;
      it->second["state"] = 1;
    }
    std::cout << "客户端请求关闭" << std::endl;
    close(fd);
    auto its = infos.find(std::to_string(fd));
    // 向真正的服务器请求关闭
    close(its->second.socket_d);
    {
      std::unique_lock<std::mutex> lock(mutex_infos);
      infos.erase(its);
    }
    {
      std::unique_lock<std::mutex> lock(mutex_epollfd);
      epoll_ctl(epollfd, EPOLL_CTL_DEL, fd,
                NULL);  // 把要监听的客户端套接字描述符从epoll实例中剔除
    }
    // 删除servers中客户端
    auto s = servers.find(std::to_string(its->second.desPort));
    auto ele =
        std::find(s->second.begin(), s->second.end(), std::to_string(fd));
    s->second.erase(ele);
  } else if (len < 0) {
    std::cout << "errno = " << errno << std::endl;
    perror("read()");
    exit(-1);
    // std::cout << "操作一个已经关闭或本身就不存在的描述字" << std::endl;
    // auto it = infos.find(std::to_string(fd));
    // close(it->second.socket_d);
    // {
    //   std::unique_lock<std::mutex> lock(mutex_infos);
    //   infos.erase(it);
    // }
  } else {
    // 检查套接字的FIN标志位，是否产生粘包
    // struct tcp_info tcpinfo;
    // socklen_t lent = sizeof(tcpinfo);
    // if (getsockopt(fd, IPPROTO_TCP, TCP_INFO, &tcpinfo, &lent) < 0) {
    //   perror("getsockopt()x");
    //   exit(-1);
    // }
    // bool finFlag = (tcpinfo.tcpi_state == TCP_CLOSE);
    // if (finFlag) {
    //   std::cout << "标志位被设置了" << std::endl;
    // }

    // 真正在传输数据
    Message message;
    // memcpy(&message, buf, sizeof(Message));
    deserializeStruct(buf, message);
    // 在红黑树中查找信息，如果不存在就需要构建信息，并放入到红黑树中
    auto it = infos.find(std::to_string(fd));
    my_int sendfd;
    std::cout << "asdfg" << std::endl;
    if (it == infos.end()) {
      // 表明在红黑树中没有查到，就需要构造一个消息节点
      Info info;
      info.desIp = message.desIp;
      info.desPort = message.desPort;
      info.socket_d =
          createSocket(info.desIp, info.desPort, 0);  // 创建之后发出连接请求
      sendfd = info.socket_d;
      myConnect(sendfd, info.desIp, info.desPort);
      // 构造套接字
      {
        std::unique_lock<std::mutex> lock(mutex_infos);
        infos[std::to_string(fd)] = info;  // 把节点信息加入到红黑树中
        // Info *info1 = new Info;
        // infos["1"] = *info1;
        // infos.insert(std::make_pair(std::to_string(fd), info));
      }
      // std::cout << getpid() << std::endl;
    } else {
      // 表明在红黑树中已经存在节点了
      sendfd = it->second.socket_d;
    }
    std::cout << "sdg" << std::endl;
    // my_int port = it->second.desPort;
    // 查询servers的信息
    auto s = servers.find(std::to_string(message.desPort));
#if FLAG
    auto ele =
        std::find(s->second.begin(), s->second.end(), std::to_string(fd));
    if (ele == s->second.end()) {
      // 表明没有找到，添加进去
      s->second.push_back(std::to_string(fd));
    }
    // 转发消息
    std::cout << "转发数据" << std::endl;
    write(sendfd, buf, BUFSIZE);
    // 任务处理完成减少fd相关的任务数量记录
    {
      std::unique_lock<std::mutex> lock(mutex_task);
      // 记录套接字相关的任务数
      // 先查询fd_tasks中是不是已经存在fd
      auto it = fd_tasks.find(fd);
      it->second["number"]--;
      std::cout << "sgsd = " << it->second["number"] << std::endl;
    }
#else
    if (s->second.empty()) {
      // 表明这台服务器还没有人连接可以连接
      s->second.push_back(std::to_string(fd));
      // 转发消息
      write(sendfd, buf, sizeof(buf));
      {
        std::unique_lock<std::mutex> lock(mutex_task);
        // 记录套接字相关的任务数
        // 先查询fd_tasks中是不是已经存在fd
        auto it = fd_tasks.find(fd);
        it->second--;
      }
    } else {
      // 表明这台服务器已经建立了连接不能再次建立连接
      // 把任务再次放入到任务池中
      relay.threadPool->enqueue(coroutineFunction, fd, relay);
    }
#endif
  }
  // }
}

void RelayServer::recvMsg() {
  if (listen(socket_d, 50) < 0) {
    perror("listen()");
    exit(-1);
  }

  // 把socket_d套接口设置成非阻塞模式
  // my_int flags = fcntl(socket_d, F_GETFL, 0);
  // fcntl(socket_d, F_SETFL, flags | O_NONBLOCK);
  // 使用epoll
  struct sockaddr_in raddr;
  epollfd = epoll_create(2);
  if (epollfd < 0) {
    perror("epoll()");
    exit(-1);
  }
  struct epoll_event event;
  event.data.fd = socket_d;
  event.events = EPOLLIN;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, socket_d, &event);

  struct epoll_event revents[REVENTSSIZE];

  while (true) {
    //
    my_int num = epoll_wait(epollfd, revents, REVENTSSIZE, -1);
    if (num < 0) {
      perror("epoll_wait()");
      exit(-1);
    }
    for (my_int i = 0; i < num; i++) {
      my_int fd = revents[i].data.fd;
      if (fd == socket_d) {
        // 表明服务器接收到了来自客户端的连接请求
        // while (true) {
        socklen_t len = sizeof(raddr);
        my_int newsd =
            accept(socket_d, (struct sockaddr *__restrict)&raddr, &len);
        if (newsd < 0) {
          if (errno == EWOULDBLOCK) {
            // 表明本次通知的待处理事件全部处理完成
            break;
          }
          perror("accept()");
          exit(-1);
        }
        char ip[16];
        inet_ntop(AF_INET, &raddr.sin_addr.s_addr, ip, sizeof(ip));
        printf("client : %s, port : %d\n", ip, ntohs(raddr.sin_port));
        if (newsd < 0) {
          perror("accept()");
          close(socket_d);
          exit(-1);
        }

        // 把socket_d套接口设置成非阻塞模式
        my_int flags = fcntl(newsd, F_GETFL, 0);
        fcntl(newsd, F_SETFL, flags | O_NONBLOCK);
        event.data.fd = newsd;
        event.events = EPOLLIN | EPOLLET;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, newsd, &event);
        // }
      } else {
        // 表明是已经建立的连接要通信，而不是客户端请求连接
        // std::cout << "添加任务 " << fd << std::endl;
        char buf[BUFSIZE];
        while (true) {
          my_int len = read(fd, buf, BUFSIZE);
          // std::cout << "len = " << len << std::endl;
          if (len < 0) break;
          // 把任务加入任务队列
          // 记录套接字相关的任务数
          // 先查询fd_tasks中是不是已经存在fd
          {
            std::unique_lock<std::mutex> lock(mutex_task);
            auto it = fd_tasks.find(fd);
            if (it == fd_tasks.end()) {
              // 表明fd_tasks中不存在fd，需要新建
              std::map<std::string, my_int> m;
              m["number"] = 0;
              m["state"] = 1;
              fd_tasks[fd] = m;
            } else {
              // 表明存在fd，需要判断fd是不是可重用的
              while (true) {
                if (it->second["state"] == 1) {
                  // 表明状态是可用的
                  it->second["number"]++;
                  break;
                }
                sleep(5);
              }
            }
          }
          threadPool->enqueue([this, buf, len, fd]() mutable {
            coroutineFunction(buf, len, fd);
          });
          if (len == 0) {
            auto it = fd_tasks.find(fd);
            it->second["state"] = 0;
            break;
          }
        }
      }
    }
  }
  close(socket_d);
  close(epollfd);
}

void Server::createSocket() {
  struct sockaddr_in laddr;
  laddr.sin_family = AF_INET;
  inet_pton(AF_INET, ip.c_str(), &laddr.sin_addr.s_addr);
  laddr.sin_port = htons(port);

  socket_d = socket(AF_INET, SOCK_STREAM, 0);

  int val = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  // setsockopt(socket_d, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));

  if (bind(socket_d, (const struct sockaddr *)&laddr, sizeof(laddr)) < 0) {
    perror("bind");
    exit(-1);
  }
  if (listen(socket_d, 50) < 0) {
    perror("listen()");
    exit(-1);
  }
}

void Server::recvTask(char *strs, my_int len, my_int fd) {
  // while (true) {
  // puts(buf);
  char buf[BUFSIZE];
  memcpy(buf, strs, BUFSIZE);
  if (len == 0) {
    // std::cout << "errno = " << errno << std::endl;
    // 客户端请求关闭，关闭客户端的套接字描述符，同时也要向服务器请求关闭，还要关闭停留在中继服务器的红黑树中的结构体信息
    auto it = fd_tasks.find(fd);
    while (it->second["number"] != 0) {
      // 表明还有任务没有执行完成不能执行关闭操作
      std::cout << "空转" << it->second["number"] << std::endl;
      sleep(5);  // 暂时让其5s空转一次
    }
    {
      std::unique_lock<std::mutex> lock(mutex_server);
      it->second["number"] = -1;
      it->second["state"] = 1;
    }
    puts("中继器请求关闭");
    close(fd);
    {
      std::unique_lock<std::mutex> lock(mutex_epollfd);
      epoll_ctl(epollfd, EPOLL_CTL_DEL, fd,
                NULL);  // 把要监听的客户端套接字描述符从epoll实例中剔除
    }
  } else if (len < 0) {
    perror("read()");
    exit(-1);
    // std::cout << "操作一个已经关闭或本身就不存在的描述字" << std::endl;
  } else {
    // 真正在传输数据
    // std::cout << "数据" << std::endl;
    Message message;
    deserializeStruct(buf, message);
    // 输出消息
    std::chrono::milliseconds dura(
        std::stoi(message.message));  // 休眠的时间是num毫秒
    puts(message.message.c_str());
    std::cout << "dura" << message.message << std::endl;
    std::this_thread::sleep_for(dura);
    {
      std::unique_lock<std::mutex> lock(mutex_server);
      auto it = fd_tasks.find(fd);
      it->second["number"]--;
    }
  }
  // }
}

void Server::recvMessage() {
  // 把socket_d套接口设置成非阻塞模式
  // my_int flags = fcntl(socket_d, F_GETFL, 0);
  // fcntl(socket_d, F_SETFL, flags | O_NONBLOCK);
  // 使用epoll
  struct sockaddr_in raddr;
  epollfd = epoll_create(2);
  if (epollfd < 0) {
    perror("epoll()");
    exit(-1);
  }
  struct epoll_event event;
  event.data.fd = socket_d;
  event.events = EPOLLIN;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, socket_d, &event);

  struct epoll_event revents[REVENTSSIZE];

  while (true) {
    //
    my_int num = epoll_wait(epollfd, revents, REVENTSSIZE, -1);
    if (num < 0) {
      perror("epoll_wait()");
      exit(-1);
    }
    for (my_int i = 0; i < num; i++) {
      my_int fd = revents[i].data.fd;
      if (fd == socket_d) {
        // 表明服务器接收到了来自客户端的连接请求
        // while (true) {
        socklen_t len = sizeof(raddr);
        my_int newsd =
            accept(socket_d, (struct sockaddr *__restrict)&raddr, &len);
        if (newsd < 0) {
          if (errno == EWOULDBLOCK) {
            // 表明本次通知的待处理事件全部处理完成
            break;
          }
          perror("accept()");
          exit(-1);
        }
        char ip[16];
        inet_ntop(AF_INET, &raddr.sin_addr.s_addr, ip, sizeof(ip));
        printf("client : %s, port : %d\n", ip, ntohs(raddr.sin_port));
        if (newsd < 0) {
          perror("accept()");
          close(socket_d);
          exit(-1);
        }
        // 把socket_d套接口设置成非阻塞模式
        my_int flags = fcntl(newsd, F_GETFL, 0);
        fcntl(newsd, F_SETFL, flags | O_NONBLOCK);
        event.data.fd = newsd;
        event.events = EPOLLIN | EPOLLET;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, newsd, &event);
        // }
      } else {
        // 表明是已经建立的连接要通信，而不是客户端请求连接

        char buf[BUFSIZE];
        while (true) {
          my_int len = read(fd, buf, BUFSIZE);
          std::cout << "len = " << len << std::endl;
          if (len < 0) break;
          // 把任务加入任务队列
          // 先查询fd_tasks中是不是已经存在fd
          // std::cout << "进入" << std::endl;
          {
            std::unique_lock<std::mutex> lock(mutex_server);
            auto it = fd_tasks.find(fd);
            if (it == fd_tasks.end()) {
              // 表明fd_tasks中不存在fd，需要新建
              std::map<std::string, my_int> m;
              m["number"] = 0;
              m["state"] = 1;
              puts("as");
              fd_tasks.insert(std::make_pair(fd, m));
            } else {
              // 表明存在fd，需要判断fd是不是可重用的
              while (true) {
                if (it->second["state"] == 1) {
                  // 表明状态是可用的
                  it->second["number"]++;
                  break;
                }
                std::cout << "暂时不可用" << std::endl;
                sleep(5);
              }
            }
            // puts("13579");
            // auto a = it->second.find("number");
            // if (a == it->second.end()) {
            //   std::cout << "空的" << std::endl;
            // }
            it = fd_tasks.find(fd);
            std::cout << "任务 = " << it->second["number"] << std::endl;
          }
          threadPool->enqueue([this, buf, len, fd]() mutable {
            // char strs[BUFSIZE];
            // strcpy(strs, buf);
            recvTask(buf, len, fd);
          });
          if (len == 0) {
            auto it = fd_tasks.find(fd);
            // 修改状态
            it->second["state"] = 0;
            break;
          }
        }
      }
    }
  }
  close(socket_d);
  close(epollfd);
}

/**
  @brief 解析信息
*/
void analysis(char *buffer, my_int bytesRead) {
  std::map<std::string, std::vector<int>> receivedMap;
  // 将字节流反序列化为 map
  std::string serializedMap(buffer, bytesRead);
  size_t startPos = 0;
  size_t endPos = serializedMap.find(";");
  while (endPos != std::string::npos) {
    std::string keyValue = serializedMap.substr(startPos, endPos - startPos);

    // 解析键
    size_t colonPos = keyValue.find(":");
    std::string key = keyValue.substr(0, colonPos);

    // 解析向量长度
    size_t commaPos = keyValue.find(",", colonPos);
    int vectorSize =
        std::stoi(keyValue.substr(colonPos + 1, commaPos - colonPos - 1));

    // 解析向量元素
    std::vector<int> vector;
    size_t elementPos = commaPos + 1;
    for (int i = 0; i < vectorSize; ++i) {
      size_t nextCommaPos = keyValue.find(",", elementPos);
      int element =
          std::stoi(keyValue.substr(elementPos, nextCommaPos - elementPos));
      vector.push_back(element);
      elementPos = nextCommaPos + 1;
    }

    // 将键值对添加到 map 中
    receivedMap[key] = vector;

    // 继续解析下一个键值对
    startPos = endPos + 1;
    endPos = serializedMap.find(";", startPos);
  }

  // 输出接收到的 map
  for (const auto &pair : receivedMap) {
    std::cout << pair.first << ": ";
    for (const auto &element : pair.second) {
      std::cout << element << " ";
    }
    std::cout << std::endl;
  }
}

/**
  @brief 查询servers的程序，输出servers的信息
*/
void searchClient() {
  struct sockaddr_in raddr;
  raddr.sin_family = AF_INET;
  inet_pton(AF_INET, serverIp, &raddr.sin_addr.s_addr);
  raddr.sin_port = htons(searchPort);

  int socket_d = socket(AF_INET, SOCK_STREAM, 0);

  int val = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  // setsockopt(socket_d, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));

  if (connect(socket_d, (const struct sockaddr *)&raddr, sizeof(raddr)) < 0) {
    perror("connect()");
    exit(-1);
  }

  char *resN = NULL;
  char buf[BUFSIZE];
  // shutdown(socket_d, SHUT_RD);
  while ((resN = (fgets(buf, BUFSIZE, stdin))) != NULL) {
    write(socket_d, buf, sizeof(buf));
    int res = -1;
    res = read(socket_d, buf, BUFSIZE);
    analysis(buf, res);
  }
  close(socket_d);
}