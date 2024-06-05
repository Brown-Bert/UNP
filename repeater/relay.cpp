#include "relay.hpp"

#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

#include "log.hpp"
#include "utils.hpp"

ThreadPool *RelayThreadPool;
int SIGANLSTOP = false;

/**
    @brief 查询线程，用户返回当前服务器的会话信息
*/
void RelayServer::searchThread() {
  struct sockaddr_in laddr, raddr;
  laddr.sin_family = AF_INET;
  inet_pton(AF_INET, SERVERIP, &laddr.sin_addr.s_addr);
  laddr.sin_port = htons(searchPort);

  my_int socket_d = socket(AF_INET, SOCK_STREAM, 0);

  int val = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  if (bind(socket_d, (const struct sockaddr *)&laddr, sizeof(laddr)) < 0) {
    Logger::getInstance()->writeLogger(
        Logger::ERROR, "bind error in relayserver searchThread");
    int state = -1;
    pthread_exit(&state);
  }
  if (listen(socket_d, 50) < 0) {
    Logger::getInstance()->writeLogger(
        Logger::ERROR, "listen error in relayserver searchThread");
    int state = -1;
    pthread_exit(&state);
  }
  socklen_t len = sizeof(raddr);
  std::cout << "查询线程启动" << std::endl;
  Logger::getInstance()->writeLogger(Logger::INFO, "查询线程启动");
  while (true) {
    my_int newsd = accept(socket_d, (struct sockaddr *)&raddr, &len);
    if (newsd < 0) {
      close(socket_d);
      int state = -1;
      Logger::getInstance()->writeLogger(
          Logger::ERROR, "accept serror in relayserver searchThread");
      pthread_exit(&state);
    }
    char ip[16];
    inet_ntop(AF_INET, &raddr.sin_addr, ip, sizeof(ip));
    char bp[100] = {'\0'};
    snprintf(bp, sizeof(bp), "client : %s, port = %d", ip,
             ntohs(raddr.sin_port));
    puts(bp);
    Logger::getInstance()->writeLogger(Logger::INFO, bp);
    char buf[BUFSIZE];
    while (true) {
      int len = read(newsd, buf, BUFSIZE);
      if (len == 0) {
        // 表明对方请求关闭
        std::cout << "查询客户端请求关闭" << std::endl;
        Logger::getInstance()->writeLogger(Logger::INFO, "查询客户端请求关闭");
        close(newsd);
        break;
      }
      // 返回信息
      // 将 map 序列化为字节流
      std::string str(buf);
      if (str == "close") {
        // 表明对方请求关闭线程
        close(socket_d);
        std::cout << "正常查询线程关闭" << std::endl;
        Logger::getInstance()->writeLogger(Logger::INFO, "正常查询线程关闭");
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
        Logger::getInstance()->writeLogger(
            Logger::ERROR, "send error in relayserver searchThread");
        int state = -1;
        pthread_exit(&state);
      }
    }
  }
  close(socket_d);
  pthread_exit(0);
}

/**
    @brief 执行优雅退出时挂的钩子函数
*/
void RelayServer::clearExit() {
  if (RelayThreadPool != nullptr) {
    delete RelayThreadPool;
    RelayThreadPool = nullptr;
  }
  servers.clear();
  MessageInfo.clear();
  close(socket_d);
  fd_tasks.clear();
}

/**
    @brief 中继服务器构造函数
*/
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
void RelayServer::createSocket(){};

/**
    @brief 中继服务器创建套接字
    @param ip 本地ip
    @param port 本地端口
    @param flag
   指定绑定本地ip和端口时需不需要指定本地端口，0：表示不需要指定本地端口，1：表示需要指定本地端口
*/
my_int RelayServer::selfCreateSocket(std::string ip, my_int port, my_int flag) {
  struct sockaddr_in laddr;
  laddr.sin_family = AF_INET;
  inet_pton(AF_INET, ip.c_str(), &laddr.sin_addr.s_addr);
  laddr.sin_port = htons(port);

  my_int fd = socket(AF_INET, SOCK_STREAM, 0);

  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  if (flag) {
    socket_d = fd;
    if (bind(fd, (const struct sockaddr *)&laddr, sizeof(laddr)) < 0) {
      Logger::getInstance()->writeLogger(
          Logger::ERROR, "bind error in relayserver selfCreateSocket");
      clearExit();
      killThread();
      exit(-1);
    }
  }
  /**
    @brief 测试中继服务器发送缓冲区大小
  */
  // int send_buffer_size;
  // socklen_t optlen = sizeof(send_buffer_size);
  // getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, &optlen);
  // std::cout << "中继服务器发送缓冲区大小 = " << send_buffer_size <<
  // std::endl;
  return fd;
}

/**
    @brief 中继服务器创建线程池
*/
void RelayServer::createPool(my_int threadNum) {
  threadPool = new ThreadPool(threadNum);
  RelayThreadPool = threadPool;
}

/**
    @brief 中继服务器和真实服务器发起连接请求
    @param fd 建立连接的套接字描述符
    @param desIp 真实服务器的ip
    @param desPort 真实服务器的端口
*/
void RelayServer::myConnect(my_int fd, std::string desIp, my_int desPort) {
  struct sockaddr_in raddr;
  raddr.sin_family = AF_INET;
  inet_pton(AF_INET, desIp.c_str(), &raddr.sin_addr.s_addr);
  raddr.sin_port = htons(desPort);

  if (connect(fd, (const struct sockaddr *)&raddr, sizeof(raddr)) < 0) {
    Logger::getInstance()->writeLogger(
        Logger::ERROR, "connect error in releyserver myConnect");
    clearExit();
    exit(-1);
  }
}

std::mutex mutex_infos;    // 会话信息的锁
std::mutex mutex_epollfd;  // epoll实例的锁
std::mutex mutex_task;     // 任务锁
std::mutex mutex_combine;  // 组包的锁

/**
    @brief 任务池任务队列中的任务函数
    @param message 需要转发的消息
    @param fd 转发的套接字描述符
*/
void RelayServer::recvTask(Message message, my_int fd) {
  int closeFlag = 0;
  /**
    @brief 测试代码 检测是否产生粘包
  */
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

  // 在红黑树中查找信息，如果不存在就需要构建信息，并放入到红黑树中
  my_int sendfd;
  {
    std::unique_lock<std::mutex> lock(mutex_infos);
    auto it = infos.find(std::to_string(fd));
    if (it == infos.end()) {
      // 表明在红黑树中没有查到，就需要构造一个消息节点
      Info info;
      info.desIp = message.desIp;
      info.desPort = message.desPort;
      info.socket_d = selfCreateSocket(info.desIp, info.desPort,
                                       0);  // 创建之后发出连接请求
      sendfd = info.socket_d;
      myConnect(sendfd, info.desIp, info.desPort);
      // 设置成非阻塞IO
      my_int flags = fcntl(info.socket_d, F_GETFL, 0);
      fcntl(info.socket_d, F_SETFL, flags | O_NONBLOCK);
      // 构造套接字
      infos.insert(std::make_pair(std::to_string(fd),
                                  info));  // 把节点信息加入到红黑树中
    } else {
      // 表明在红黑树中已经存在节点了
      sendfd = it->second.socket_d;
    }
  }
  // 查询servers的信息
  auto s = servers.find(std::to_string(message.desPort));
  if (s == servers.end()) {
    // 表明没有找到这个服务器
    Logger::getInstance()->writeLogger(
        Logger::ERROR, "没有找到这个服务器 in relayserver recvTask");
  }
#if FLAG
  auto ele = std::find(s->second.begin(), s->second.end(), std::to_string(fd));
  if (ele == s->second.end()) {
    // 表明没有找到，添加进去
    s->second.push_back(std::to_string(fd));
  }
  // 转发消息
  {
    std::unique_lock<std::mutex> lock(mutex_send);
    my_int res_len = sendMessage(sendfd, message);
    if (res_len == 0) {
      threadPool->enqueue(
          [this, strs = message, tmpfd = fd]() { recvTask(strs, tmpfd); });
      return;
    } else if (res_len != BUFSIZE) {
      Logger::getInstance()->writeLogger(
          Logger::ERROR,
          "sendMessage 转发消息后返回值异常 in relayserver recvTask");
    }
  }
  Logger::getInstance()->writeLogger(Logger::INFO, "数据转发成功");
  // 任务处理完成减少fd相关的任务数量记录
  {
    std::unique_lock<std::mutex> lock(mutex_task);
    // 记录套接字相关的任务数
    // 先查询fd_tasks中是不是已经存在fd
    auto it = fd_tasks.find(fd);
    if (it == fd_tasks.end()) {
      Logger::getInstance()->writeLogger(
          Logger::ERROR, "在fd_tasks中找不到 in relayserver recvTask");
    }
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
    relay.threadPool->enqueue(recvTask, fd, relay);
  }
#endif
  // 判断是不是要执行关闭
  if (closeFlag) {
    Logger::getInstance()->writeLogger(
        Logger::INFO, "线程中真正关闭 in relayserver recvTask");
    {
      auto its = infos.find(std::to_string(fd));
      if (its == infos.end()) {
        Logger::getInstance()->writeLogger(
            Logger::ERROR, "infos中找不到 in relayserver recvTask");
      }
      // 删除servers中客户端
      auto s = servers.find(std::to_string(its->second.desPort));
      if (s == servers.end()) {
        Logger::getInstance()->writeLogger(
            Logger::ERROR, "servers中找不到 in relayserver recvTask");
      }
      auto ele =
          std::find(s->second.begin(), s->second.end(), std::to_string(fd));
      if (ele == s->second.end()) {
        Logger::getInstance()->writeLogger(
            Logger::ERROR, "s->second中找不到 in relayserver recvTask");
      }
      s->second.erase(ele);
      // 向真正的服务器请求关闭
      close(its->second.socket_d);
      std::unique_lock<std::mutex> lock(mutex_infos);
      infos.erase(its);
      close(fd);
    }
    {
      std::unique_lock<std::mutex> lock(mutex_combine);
      auto it = MessageInfo.find(fd);
      if (it == MessageInfo.end()) {
        Logger::getInstance()->writeLogger(
            Logger::ERROR,
            "删除连接时MessageInfo中找不到 in relayserver recvTask");
      }
      MessageInfo.erase(it);
    }
  }
}

std::mutex mutex_epoll;
void RelayServer::recvMessage() {
  if (listen(socket_d, 100) < 0) {
    Logger::getInstance()->writeLogger(
        Logger::ERROR, "listen error in relayserver recvMessage");
    clearExit();
    killThread();
    int state = -1;
    pthread_exit(&state);
  }
  struct sockaddr_in raddr;
  struct epoll_event event;
  my_int epollfd_t = epoll_create(4);
  std::vector<my_int> v;
  epoll_all[epollfd_t] = v;
  if (epollfd_t < 0) {
    Logger::getInstance()->writeLogger(
        Logger::ERROR, "epoll error in relayserver recvMessage");
    clearExit();
    killThread();
    int state = -1;
    pthread_exit(&state);
  }
  if (flaglock) {
    flaglock = false;
    event.data.fd = socket_d;
    event.events = EPOLLIN;
    epoll_ctl(epollfd_t, EPOLL_CTL_ADD, socket_d, &event);
  }
  auto start_time =
      std::chrono::system_clock::now();  // 统计包转发数量的开始时间
  struct epoll_event revents[REVENTSSIZE];
  std::cout << "中继服务器启动" << std::endl;
  Logger::getInstance()->writeLogger(Logger::INFO, "中继服务器启动");
  while (true) {
    my_int num = epoll_wait(epollfd_t, revents, REVENTSSIZE, -1);
    if (SIGANLSTOP) {
      // 信号中断
      Logger::getInstance()->writeLogger(Logger::INFO, "中继服务器被信号中断");
      clearExit();
      close(epollfd_t);
      pthread_exit(0);
    }
    if (num < 0) {
      Logger::getInstance()->writeLogger(
          Logger::ERROR, "epoll_wait error in relayserver recvMessage");
      clearExit();
      close(epollfd_t);
      pthread_exit(0);
    }
    for (my_int i = 0; i < num; i++) {
      my_int fd = revents[i].data.fd;
      if (fd == socket_d) {
        // 表明服务器接收到了来自客户端的连接请求
        // while (true) {
        std::cout << "客户端请求连接" << std::endl;
        Logger::getInstance()->writeLogger(Logger::INFO, "客户端请求连接");
        socklen_t len = sizeof(raddr);
        my_int newsd = accept(fd, (struct sockaddr *__restrict)&raddr, &len);
        if (newsd < 0) {
          if (errno == EWOULDBLOCK || errno == EAGAIN) {
            // 表明本次通知的待处理事件全部处理完成
            Logger::getInstance()->writeLogger(Logger::INFO, "事件处理完成");
            break;
          }
          Logger::getInstance()->writeLogger(
              Logger::ERROR, "accept error in relayserver recvMessage");
          clearExit();
          close(epollfd_t);
          int state = -1;
          pthread_exit(&state);
        }
        char ip[16];
        inet_ntop(AF_INET, &raddr.sin_addr.s_addr, ip, sizeof(ip));
        char bp[100] = {'\0'};
        snprintf(bp, sizeof(bp), "client : %s, port : %d", ip,
                 ntohs(raddr.sin_port));
        puts(bp);
        Logger::getInstance()->writeLogger(Logger::INFO, bp);

        // 把socket_d套接口设置成非阻塞模式
        my_int flags = fcntl(newsd, F_GETFL, 0);
        fcntl(newsd, F_SETFL, flags | O_NONBLOCK);
        event.data.fd = newsd;
        event.events = EPOLLIN;
        {
          std::unique_lock<std::mutex> lock(mutex_epoll);
          auto tp = fd_epollfd.find(newsd);
          if (tp == fd_epollfd.end()) {
            int ep_f = 0;
            for (auto &ep : epoll_all) {
              if (ep.second.size() < REVENTSSIZE) {
                // 构建包消息
                std::vector<std::vector<std::string>> v;
                {
                  std::unique_lock<std::mutex> lock(mutex_combine);
                  MessageInfo.insert({newsd, v});
                }
                // 先查询fd_tasks中是不是已经存在fd
                {
                  std::unique_lock<std::mutex> lock(mutex_task);
                  // fd_tasks中不存在fd，需要新建
                  std::map<std::string, my_int> m;
                  m["number"] = 1;
                  m["state"] = 1;
                  m["isclose"] = 0;
                  fd_tasks[newsd] = m;
                }
                epoll_ctl(ep.first, EPOLL_CTL_ADD, newsd, &event);
                ep.second.push_back(newsd);
                fd_epollfd[newsd] = ep.first;
                ep_f = 1;
                break;
              }
            }
            if (ep_f == 0) {
              puts("epoll监听满了");
              Logger::getInstance()->writeLogger(Logger::WARNING,
                                                 "epoll监听满了");
            }
          }
        }
      } else {
        // 表明是已经建立的连接要通信，而不是客户端请求连接
        // 把数据读出来
        char buf[BUFSIZE];
        int len = 0, n = 0;
        // 持续读取数据，知道读取BUfSIZE大小的数据
        while (len < BUFSIZE) {
          n = read(fd, buf + len, BUFSIZE - len);
          if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
              // 表明没有数据可读取，非阻塞IO立即返回
              errno = 0;
              continue;
            } else {
              perror("持续读取错误");
              Logger::getInstance()->writeLogger(Logger::ERROR, "持续读取错误");
              break;
            }
          } else if (n == 0) {
            perror("持续读取到结尾");
            Logger::getInstance()->writeLogger(Logger::INFO, "持续读取到结尾");
            break;
          }
          len += n;
        }
        {
          std::unique_lock<std::mutex> lock(mutex_count);
          if (count_pkg == 0) {
            // 获取当前时间点
            start_time = std::chrono::system_clock::now();
          }
          count_pkg++;
        }
        if (len == 0) {
          puts("客户端请求关闭");
          Logger::getInstance()->writeLogger(Logger::INFO, "客户端请求关闭");
          auto end_time = std::chrono::system_clock::now();
          auto delaytime = std::chrono::duration_cast<std::chrono::seconds>(
              end_time - start_time);
          auto delaytimecount = delaytime.count() == 0 ? 1 : delaytime.count();
          std::string outputStr = "平均每秒处理的包数量 = " +
                                  std::to_string(count_pkg / delaytimecount) +
                                  " 时间 = " + std::to_string(delaytimecount);
          std::cout << outputStr << std::endl;
          Logger::getInstance()->writeLogger(Logger::INFO, outputStr);
          // 表明客户端请求关闭
          auto it = fd_tasks.find(fd);
          if (it == fd_tasks.end()) {
            Logger::getInstance()->writeLogger(
                Logger::ERROR, "出现异常情况 in relayserver recvMessage");
          }
          {
            std::unique_lock<std::mutex> lock(mutex_task);
            // 修改fd_tasks中的记录
            if (it->second["number"] > 1) {
              // 表明这个请求关闭的套接字还有其他任务没有完成
              it->second["isclose"] = 1;
              it->second["state"] = 0;
              {
                std::unique_lock<std::mutex> lock(mutex_epollfd);
                if (epoll_ctl(epollfd_t, EPOLL_CTL_DEL, fd,
                              NULL) <
                    0) {  // 把要监听的客户端套接字描述符从epoll实例中剔除
                  Logger::getInstance()->writeLogger(
                      Logger::ERROR,
                      "first epoll实例中删除失败 in relayserver recvMessage");
                }
                auto d_fd = fd_epollfd.find(fd);
                auto iter = epoll_all.find(d_fd->second);
                auto p =
                    std::find(iter->second.begin(), iter->second.end(), fd);
                iter->second.erase(p);
                fd_epollfd.erase(d_fd);
              }
            } else {
              // 就在epoll中执行关闭
              Logger::getInstance()->writeLogger(
                  Logger::INFO, "epoll中真正关闭 in relayserver recvMessage");
              it->second["state"] = 0;
              auto its = infos.find(std::to_string(fd));
              // 如果没有找到，表明是刚建立连接还没有数据通信就请求关闭
              if (its != infos.end()) {
                // 删除servers中客户端
                auto s = servers.find(std::to_string(its->second.desPort));
                if (s == servers.end()) {
                  Logger::getInstance()->writeLogger(
                      Logger::ERROR,
                      "servers中找不到 in relayserver recvMessage");
                }
                auto ele = std::find(s->second.begin(), s->second.end(),
                                     std::to_string(fd));
                // 如果没有找到，表明是刚建立连接还没有数据通信就请求关闭
                if (ele != s->second.end()) {
                  s->second.erase(ele);
                }
                // 向真正的服务器请求关闭
                close(its->second.socket_d);
                {
                  std::unique_lock<std::mutex> lock(mutex_infos);
                  infos.erase(its);
                }
              }
              {
                std::unique_lock<std::mutex> lock(mutex_epollfd);
                if (epoll_ctl(epollfd_t, EPOLL_CTL_DEL, fd,
                              NULL) <
                    0) {  // 把要监听的客户端套接字描述符从epoll实例中剔除
                  Logger::getInstance()->writeLogger(
                      Logger::ERROR,
                      "second epoll实例中删除失败 in relayserver recvMessage");
                }
                auto d_fd = fd_epollfd.find(fd);
                auto iter = epoll_all.find(d_fd->second);
                auto p =
                    std::find(iter->second.begin(), iter->second.end(), fd);
                iter->second.erase(p);
                fd_epollfd.erase(d_fd);
              }
              close(fd);
              // 删除MessageInfo中的消息
              {
                std::unique_lock<std::mutex> lock(mutex_combine);
                auto it = MessageInfo.find(fd);
                if (it == MessageInfo.end()) {
                }
                MessageInfo.erase(it);
              }
            }
          }
        } else if (len < 0) {
          std::cout << "epoll读取异常" << std::endl;
          Logger::getInstance()->writeLogger(
              Logger::ERROR, "epoll读取异常 in relayserver recvMessage");
        } else {
          //消息处理，每个线程接收到的消息只是一个包，有些发送方的消息不只是只有一个包，所有线程之间要协作组合包
          // 1、先判断这个包是不是不需要和其他包组合
          Message message;
          deserializeStruct(buf, message);
          threadPool->enqueue(
              [this, strs = message, tmpfd = fd]() { recvTask(strs, tmpfd); });
        }
      }
    }
  }
  close(socket_d);
  close(epollfd);
}
