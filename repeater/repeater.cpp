#include "repeater.h"

#include <arpa/inet.h>
#include <fcntl.h>
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
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "threadPool.h"
ThreadPool *RelayThreadPool;

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
  buffer += messageLength;

  // 序列化包的大小
  memcpy(buffer, &message.packageSize, sizeof(int));
  buffer += sizeof(int);

  // 序列化包的编号
  memcpy(buffer, &message.packageNum, sizeof(int));
  buffer += sizeof(int);
}

// 反序列化字节流为结构体
const char *deserializeStruct(const char *buffer, Message &message) {
  // 暂存buffer的指针
  const char *tmp = buffer;
  // 反序列化发送方字符串成员的长度和字符数据
  int messageLength;
  memcpy(&messageLength, buffer, sizeof(int));
  buffer += sizeof(int);
  // std::cout << "ggg = " << messageLength << std::endl;
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
  buffer += messageLength;

  // 反序列化接收方的包大小
  memcpy(&message.packageSize, buffer, sizeof(int));
  buffer += sizeof(int);

  // 反序列化接收方的包编号
  memcpy(&message.packageNum, buffer, sizeof(int));
  buffer += sizeof(int);

  return tmp;
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
    close(socket_d);
    exit(-1);
  }
}

void Client::setIpAndPort() {
  struct sockaddr_in laddr;
  socklen_t len = sizeof(laddr);
  if (getsockname(socket_d, (struct sockaddr *)&laddr, &len) < 0) {
    perror("getsockname()");
    close(socket_d);
    exit(-1);
  }
  char ipt[16];
  inet_ntop(AF_INET, &laddr.sin_addr.s_addr, ipt, sizeof(ipt));
  ip = ipt;
  port = laddr.sin_port;
}

void Client::sendMessage(std::string desIp, my_int desPort, std::string msg) {
  // 如果消息够长，则会造成缓冲区溢出，而且自己定义的数据结构在接收方并不能按照预计的那样去读取，
  // 所以需要将具体的消息手动切片或者补齐
  // 发送信息
  Message message;
  message.sourceIp = ip;
  message.sourcePort = port;
  message.desIp = desIp;
  message.desPort = desPort;
  message.packageSize = msg.size();
  int len =
      BUFSIZE - (sizeof(ip) + sizeof(port) + sizeof(desIp) + sizeof(desPort) +
                 sizeof(message.packageSize) + sizeof(message.packageNum) + 1);
  int count = 0;
  while (true) {
    if (msg.size() > len) {
      message.packageNum = count;
      count++;
      message.message = msg.substr(0, len);
      msg = msg.substr(len);
      // 将结构体序列化
      char buf[BUFSIZE] = {'\0'};
      // memcpy(buf, &message, sizeof(message));
      serializeStruct(message, buf);
      buf[BUFSIZE - 1] = '\0';
      write(socket_d, buf, BUFSIZE);
      // send(socket_d, buf, BUFSIZE, 0);
    } else {
      message.message = msg;
      message.packageNum = count;
      // 将结构体序列化
      char buf[BUFSIZE] = {'\0'};
      // memcpy(buf, &message, sizeof(message));
      serializeStruct(message, buf);
      buf[BUFSIZE - 1] = '\0';
      write(socket_d, buf, BUFSIZE);
      // send(socket_d, buf, BUFSIZE, 0);
      break;
    }
  }
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
    int state = -1;
    pthread_exit(&state);
  }
  if (listen(socket_d, 50) < 0) {
    perror("listen()");
    int state = -1;
    pthread_exit(&state);
  }
  socklen_t len = sizeof(raddr);
  std::cout << "查询线程启动" << std::endl;
  while (true) {
    my_int newsd = accept(socket_d, (struct sockaddr *)&raddr, &len);
    if (newsd < 0) {
      close(socket_d);
      int state = -1;
      std::cout << "查询线程关闭" << std::endl;
      pthread_exit(&state);
    }
    char ip[16];
    inet_ntop(AF_INET, &raddr.sin_addr, ip, sizeof(ip));
    printf("client : %s, port = %d\n", ip, ntohs(raddr.sin_port));
    char buf[BUFSIZE];
    while (true) {
      int len = read(newsd, buf, BUFSIZE);
      if (len == 0) {
        // 表明对方请求关闭
        std::cout << "查询客户端请求关闭" << std::endl;
        close(newsd);
        break;
      }
      // 返回信息
      // 将 map 序列化为字节流
      std::string str(buf);
      if (str == "close") {
        // 表明对方请求关闭线程
        close(socket_d);
        std::cout << "查询线程关闭" << std::endl;
        int state = 0;
        pthread_exit(&state);
      }
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
        int state = -1;
        pthread_exit(&state);
      }
    }
  }
  close(socket_d);
  pthread_exit(0);
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
      close(fd);
      delete RelayThreadPool;
      servers.clear();
      killThread();
      exit(-1);
    }
  }
  return fd;
}

void RelayServer::createPool(my_int threadNum) {
  threadPool = new ThreadPool(threadNum);
  RelayThreadPool = threadPool;
}

void RelayServer::myConnect(my_int fd, std::string desIp, my_int desPort) {
  struct sockaddr_in raddr;
  raddr.sin_family = AF_INET;
  inet_pton(AF_INET, desIp.c_str(), &raddr.sin_addr.s_addr);
  raddr.sin_port = htons(desPort);
  // setsockopt(socket_d, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
  std::cout << "port = " << desPort << " " << desIp << std::endl;

  if (connect(fd, (const struct sockaddr *)&raddr, sizeof(raddr)) < 0) {
    perror("connect()");
    exit(-1);
  }
}

std::mutex mutex_infos;
std::mutex mutex_epollfd;
std::mutex mutex_task;

void RelayServer::coroutineFunction(char *strs, my_int fd) {
  // while (true) {
  int closeFlag = 0;
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
  auto deletePointer = deserializeStruct(strs, message);
  // 在红黑树中查找信息，如果不存在就需要构建信息，并放入到红黑树中
  auto it = infos.find(std::to_string(fd));
  my_int sendfd;
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
  // 查询servers的信息
  auto s = servers.find(std::to_string(message.desPort));
  if (s == servers.end()) {
    // 表明没有找到这个服务器
    std::cout << "没有找到这个服务器 = " << message.desPort << std::endl;
  }
#if FLAG
  auto ele = std::find(s->second.begin(), s->second.end(), std::to_string(fd));
  if (ele == s->second.end()) {
    // 表明没有找到，添加进去
    s->second.push_back(std::to_string(fd));
  }
  // 转发消息
  // std::cout << "转发数据到" << sendfd << std::endl;
  write(sendfd, strs, BUFSIZE);
  delete[] deletePointer;
  // send(sendfd, strs, BUFSIZE, 0);
  // 任务处理完成减少fd相关的任务数量记录
  {
    std::unique_lock<std::mutex> lock(mutex_task);
    // 记录套接字相关的任务数
    // 先查询fd_tasks中是不是已经存在fd
    auto it = fd_tasks.find(fd);
    it->second["number"]--;
    if (it->second["isclose"] == 1) {
      closeFlag = 1;
      it->second["isclose"] = 0;
    }
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
  // 判断是不是要执行关闭
  if (closeFlag) {
    auto its = infos.find(std::to_string(fd));
    // 向真正的服务器请求关闭
    std::cout << "线程中真正关闭" << std::endl;
    close(its->second.socket_d);
    {
      std::unique_lock<std::mutex> lock(mutex_infos);
      infos.erase(its);
    }
    close(fd);
    // 删除servers中客户端
    auto s = servers.find(std::to_string(its->second.desPort));
    auto ele =
        std::find(s->second.begin(), s->second.end(), std::to_string(fd));
    s->second.erase(ele);
  }
  // }
}

void RelayServer::recvMsg() {
  if (listen(socket_d, 50) < 0) {
    perror("listen()");
    close(socket_d);
    servers.clear();
    delete RelayThreadPool;
    killThread();
    exit(-1);
  }

  // 把socket_d套接口设置成非阻塞模式
  // my_int flags = fcntl(socket_d, F_GETFL, 0);
  // fcntl(socket_d, F_SETFL, flags | O_NONBLOCK);
  // 使用epoll
  struct sockaddr_in raddr;
  epollfd = epoll_create(4);
  if (epollfd < 0) {
    perror("epoll()");
    close(socket_d);
    servers.clear();
    delete RelayThreadPool;
    killThread();
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
      servers.clear();
      close(socket_d);
      close(epollfd);
      exit(0);
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
          servers.clear();
          close(socket_d);
          close(epollfd);
          exit(-1);
        }
        char ip[16];
        inet_ntop(AF_INET, &raddr.sin_addr.s_addr, ip, sizeof(ip));
        printf("client : %s, port : %d\n", ip, ntohs(raddr.sin_port));

        // 把socket_d套接口设置成非阻塞模式
        my_int flags = fcntl(newsd, F_GETFL, 0);
        fcntl(newsd, F_SETFL, flags | O_NONBLOCK);
        event.data.fd = newsd;
        event.events = EPOLLIN;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, newsd, &event);
        // }
      } else {
        // 表明是已经建立的连接要通信，而不是客户端请求连接
        // 把数据读出来
        char buf[BUFSIZE];
        // int len = read(fd, buf, BUFSIZE);
        int len = 0, n = 0;
        // 持续读取数据，知道读取BUfSIZE大小的数据
        while (len < BUFSIZE) {
          n = read(fd, buf + len, BUFSIZE - len);
          if (n < 0) {
            if (errno == EWOULDBLOCK) {
              // 表明没有数据可读取，非阻塞IO立即返回
              continue;
            }
            perror("持续读取错误");
            break;
          } else if (n == 0) {
            perror("持续读取到结尾");
            break;
          }
          len += n;
        }
        if (len == 0) {
          // 表明客户端请求关闭
          auto it = fd_tasks.find(fd);
          if (it == fd_tasks.end()) {
            std::cout << "出现异常情况" << std::endl;
          }
          {
            std::unique_lock<std::mutex> lock(mutex_task);
            // 修改fd_tasks中的记录
            if (it->second["number"] > 0) {
              // 表明这个请求关闭的套接字还有其他任务没有完成
              it->second["isclose"] = 1;
              it->second["state"] = 0;
              {
                std::unique_lock<std::mutex> lock(mutex_epollfd);
                if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd,
                              NULL) <
                    0) {  // 把要监听的客户端套接字描述符从epoll实例中剔除
                  std::cerr << "删除失败" << std::endl;
                }
              }
            } else {
              // 就在epoll中执行关闭
              std::cout << "epoll中真正关闭" << std::endl;
              it->second["state"] = 0;
              auto its = infos.find(std::to_string(fd));
              // 向真正的服务器请求关闭
              close(its->second.socket_d);
              {
                std::unique_lock<std::mutex> lock(mutex_infos);
                infos.erase(its);
              }
              {
                std::unique_lock<std::mutex> lock(mutex_epollfd);
                if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd,
                              NULL) <
                    0) {  // 把要监听的客户端套接字描述符从epoll实例中剔除
                  std::cerr << "删除失败" << std::endl;
                }
              }
              close(fd);
              // 删除servers中客户端
              auto s = servers.find(std::to_string(its->second.desPort));
              auto ele = std::find(s->second.begin(), s->second.end(),
                                   std::to_string(fd));
              s->second.erase(ele);
            }
          }
          puts("客户端请求关闭");
        } else if (len < 0) {
          std::cout << "epoll读取异常" << std::endl;
        } else {
          // std::cout << "添加任务 " << fd << std::endl;
          // 把任务加入任务队列
          // 记录套接字相关的任务数
          // 先查询fd_tasks中是不是已经存在fd
          {
            std::unique_lock<std::mutex> lock(mutex_task);
            auto it = fd_tasks.find(fd);
            if (it == fd_tasks.end()) {
              // 表明fd_tasks中不存在fd，需要新建
              std::map<std::string, my_int> m;
              m["number"] = 1;
              m["state"] = 1;
              m["isclose"] = 0;
              fd_tasks[fd] = m;
            } else {
              // 表明存在fd，需要判断fd是不是可重用的
              if (it->second["state"] == 1) {
                // 表明状态是可用的
                it->second["number"]++;
              } else {
                // 不可重用（此种情况是已经被用过，但是用完了）
                it->second["state"] = 1;
                it->second["number"] = 1;
                it->second["isclose"] = 0;
              }
            }
          }
          char *tmp = new char[BUFSIZE];
          memcpy(tmp, buf, BUFSIZE);
          threadPool->enqueue([this, strs = tmp, tmpfd = fd]() mutable {
            coroutineFunction(strs, tmpfd);
          });
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
    close(socket_d);
    exit(-1);
  }
  if (listen(socket_d, 50) < 0) {
    perror("listen()");
    close(socket_d);
    exit(-1);
  }
}

void Server::recvTask(char *strs, my_int fd) {
  // while (true) {
  int closeFlag = 0;
  // 真正在传输数据
  // std::cout << "数据" << std::endl;
  Message message;
  auto deletePointer = deserializeStruct(strs, message);
  // 消息解析完成
  delete[] deletePointer;
  // 消息处理，每个线程接收到的消息只是一个包，有些发送方的消息不只是只有一个包，所有线程之间要协作组合包
  // 1、先判断这个包是不是不需要和其他包组合
  if (message.packageSize == message.message.size()) {
    std::cout << "不需要组合，消息 = " << message.message << std::endl;
  } else {
    // 2、需要组合，再次判断是不是本次消息的最后一个包，如果是那么就要把所有包拿出来组合消息，然后打印到终端，
    // 如果不是最后一个包，那么就需要把包插入到合适的位置
    auto it = MessageInfo.find(fd);
    int len = 0;
    for (auto p : it->second) {
      len += p[1].size();
    }
    if (len == message.packageSize) {
      // 表明是最后一个包
      // 组装包消息
      std::string pkgStrs = "";
      for (auto pkg : it->second) {
        pkgStrs += pkg[1];
      }
      // 组装完成，打印消息
      std::cout << "组合之后，消息 = " << pkgStrs << std::endl;
      // 删除MessageInfo中的包消息
      it->second.clear();
    } else {
      // 表明不是最后一个包，需要把包插入到合适的位置
      std::vector<std::string> v;
      v.push_back(std::to_string(message.packageNum));
      v.push_back(message.message);
      if (it->second.size() == 0) {
        // 直接插入
        it->second.push_back(v);
      } else {
        int f = 0;
        for (auto pkg = it->second.begin(); pkg != it->second.end(); ++pkg) {
          if (message.packageNum < std::stoi((*pkg)[0])) {
            f = 1;
            it->second.insert(pkg, v);
          }
        }
        if (f == 0) {
          // 直接在末尾插入
          it->second.push_back(v);
        }
      }
    }
  }
  {
    std::unique_lock<std::mutex> lock(mutex_task);
    auto it = fd_tasks.find(fd);
    it->second["number"]--;
    if (it->second["isclose"] == 1) {
      closeFlag = 1;
      it->second["isclose"] = 0;
    }
  }
  if (closeFlag) {  // 删除MessageInfo中的消息
    auto it = MessageInfo.find(fd);
    MessageInfo.erase(it);
    close(fd);
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
    close(socket_d);
    int state = -1;
    pthread_exit(&state);
  }
  struct epoll_event event;
  event.data.fd = socket_d;
  event.events = EPOLLIN;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, socket_d, &event);

  struct epoll_event revents[REVENTSSIZE];
  std::cout << "服务器启动" << std::endl;

  while (true) {
    //
    my_int num = epoll_wait(epollfd, revents, REVENTSSIZE, -1);
    if (num < 0) {
      perror("epoll_wait()");
      close(socket_d);
      close(epollfd);
      std::cout << "epoll_wait被中断的服务器关闭" << std::endl;
      int state = -1;
      pthread_exit(&state);
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
          close(socket_d);
          close(epollfd);
          std::cout << "accept被中断的服务器关闭" << std::endl;
          perror("accept()");
          int state = -1;
          pthread_exit(&state);
        }
        char ip[16];
        inet_ntop(AF_INET, &raddr.sin_addr.s_addr, ip, sizeof(ip));
        printf("client : %s, port : %d\n", ip, ntohs(raddr.sin_port));
        // 把socket_d套接口设置成非阻塞模式
        my_int flags = fcntl(newsd, F_GETFL, 0);
        fcntl(newsd, F_SETFL, flags | O_NONBLOCK);
        event.data.fd = newsd;
        event.events = EPOLLIN;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, newsd, &event);
        // 构建包消息
        std::vector<std::vector<std::string>> v;
        MessageInfo.insert({newsd, v});
        // }
      } else {
        // 表明是已经建立的连接要通信，而不是客户端请求连接

        char buf[BUFSIZE];
        int len = 0, n = 0;
        // 持续读取数据，知道读取BUfSIZE大小的数据
        while (true) {
          n = read(fd, buf + len, BUFSIZE - len);
          if (n < 0) {
            if (errno == EWOULDBLOCK) {
              // 表明没有数据可读取，非阻塞IO立即返回
              continue;
            }
            perror("持续读取错误");
            break;
          } else if (n == 0) {
            perror("持续读取到结尾");
            break;
          }
          len += n;
        }
        std::string closestr(buf);
        if (closestr == "close") {
          // 服务器关闭自己
          close(fd);
          close(epollfd);
          std::cout << "正常的服务器关闭" << std::endl;
          int state = 0;
          pthread_exit(&state);
        }
        if (len == 0) {
          // 表明客户端请求关闭
          auto it = fd_tasks.find(fd);
          if (it == fd_tasks.end()) {
            std::cout << "出现异常情况" << std::endl;
          }
          {
            std::unique_lock<std::mutex> lock(mutex_task);
            // 修改fd_tasks中的记录
            if (it->second["number"] > 0) {
              // 表明这个请求关闭的套接字还有其他任务没有完成
              it->second["isclose"] = 1;
              it->second["state"] = 0;
              {
                std::unique_lock<std::mutex> lock(mutex_epollfd);
                epoll_ctl(
                    epollfd, EPOLL_CTL_DEL, fd,
                    NULL);  // 把要监听的客户端套接字描述符从epoll实例中剔除
              }
            } else {
              // 可以在此线程中执行关闭
              it->second["state"] = 0;
              {
                std::unique_lock<std::mutex> lock(mutex_epollfd);
                epoll_ctl(
                    epollfd, EPOLL_CTL_DEL, fd,
                    NULL);  // 把要监听的客户端套接字描述符从epoll实例中剔除
              }
              close(fd);
              // 删除MessageInfo中的消息
              auto it = MessageInfo.find(fd);
              MessageInfo.erase(it);
            }
          }
          puts("中继器请求关闭");
        } else if (len < 0) {
          std::cout << "epoll读取异常" << std::endl;
        } else {
          // while (true) {
          // 把任务加入任务队列
          // 先查询fd_tasks中是不是已经存在fd
          // std::cout << "进入" << std::endl;
          {
            std::unique_lock<std::mutex> lock(mutex_task);
            auto it = fd_tasks.find(fd);
            if (it == fd_tasks.end()) {
              // 表明fd_tasks中不存在fd，需要新建
              std::map<std::string, my_int> m;
              m["number"] = 1;
              m["state"] = 1;
              m["isclose"] = 0;
              fd_tasks[fd] = m;
            } else {
              // 表明存在fd，需要判断fd是不是可重用的
              if (it->second["state"] == 1) {
                // 表明状态是可用的
                it->second["number"]++;
              } else {
                it->second["state"] = 1;
                it->second["number"] = 1;
                it->second["isclose"] = 0;
              }
            }
          }
          char *tmp = new char[BUFSIZE];
          memcpy(tmp, buf, BUFSIZE);
          threadPool->enqueue(
              [this, strs = tmp, fd]() mutable { recvTask(strs, fd); });
        }
      }
    }
  }
  close(socket_d);
  close(epollfd);
  int state = 0;
  pthread_exit(&state);
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
  inet_pton(AF_INET, SERVERIP, &raddr.sin_addr.s_addr);
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
  char buf[BUFSIZE] = {'\0'};
  // shutdown(socket_d, SHUT_RD);
  while ((resN = (fgets(buf, BUFSIZE, stdin))) != NULL) {
    // 去除换行符
    buf[strcspn(buf, "\n")] = '\0';
    std::string strs(buf);
    if (strs == "search") {
      write(socket_d, buf, sizeof(buf));
      int res = -1;
      res = read(socket_d, buf, BUFSIZE);
      analysis(buf, res);
    } else if (strs == "close") {
      break;
    } else {
      std::cout << "输入有误" << std::endl;
    }
  }
  close(socket_d);
}

// 中继器自己杀死这个分离的线程
void killThread() {
  struct sockaddr_in raddr;
  raddr.sin_family = AF_INET;
  inet_pton(AF_INET, SERVERIP, &raddr.sin_addr.s_addr);
  raddr.sin_port = htons(searchPort);

  int socket_d = socket(AF_INET, SOCK_STREAM, 0);

  int val = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  // setsockopt(socket_d, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));

  if (connect(socket_d, (const struct sockaddr *)&raddr, sizeof(raddr)) < 0) {
    perror("connect()");
    exit(-1);
  }
  // 休眠毫秒级别
  // std::this_thread::sleep_for(std::chrono::milliseconds(50));
  char buf[6] = {'c', 'l', 'o', 's', 'e', '\0'};
  write(socket_d, buf, 6);
  close(socket_d);
}

void Server::killSelf() {
  struct sockaddr_in raddr;
  raddr.sin_family = AF_INET;
  inet_pton(AF_INET, ip.c_str(), &raddr.sin_addr.s_addr);
  raddr.sin_port = htons(port);

  int socket_d = socket(AF_INET, SOCK_STREAM, 0);

  int val = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  // setsockopt(socket_d, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));

  if (connect(socket_d, (const struct sockaddr *)&raddr, sizeof(raddr)) < 0) {
    perror("connect()");
    close(socket_d);
    exit(-1);
  }
  // 休眠毫秒级别
  // std::this_thread::sleep_for(std::chrono::milliseconds(50));
  char buf[BUFSIZE] = {'\0'};
  const char *str = "close\0";
  strcpy(buf, str);
  write(socket_d, buf, BUFSIZE);
  close(socket_d);
}