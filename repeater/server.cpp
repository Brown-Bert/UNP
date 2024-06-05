#include "server.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "log.hpp"
#include "utils.hpp"

/**
    @brief 创建服务端网络套接字描述符
*/
void Server::createSocket() {
  struct sockaddr_in laddr;
  laddr.sin_family = AF_INET;
  inet_pton(AF_INET, ip.c_str(), &laddr.sin_addr.s_addr);
  laddr.sin_port = htons(port);

  socket_d = socket(AF_INET, SOCK_STREAM, 0);

  int val = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  if (bind(socket_d, (const struct sockaddr *)&laddr, sizeof(laddr)) < 0) {
    perror("bind");
    Logger::getInstance()->writeLogger(Logger::ERROR,
                                       "bind error in server createSocket");
    close(socket_d);
    exit(-1);
  }
  if (listen(socket_d, 50) < 0) {
    perror("listen()");
    Logger::getInstance()->writeLogger(Logger::ERROR,
                                       "listen error in server createSocket");
    close(socket_d);
    exit(-1);
  }
}

std::mutex mutex_delay;    // 延迟锁
std::mutex mutex_task;     // 任务锁
std::mutex mutex_combine;  // 组包的锁
std::mutex mutex_epollfd;  // epoll实例的锁

/**
    @brief 任务池任务队列中的任务函数
    @param message 需要转发的消息
    @param fd 转发的套接字描述符
*/
void Server::recvTask(Message message, my_int fd) {
  // 计算消息时延
  auto starttime = strToTime(message.timestr);
  // 获取当前时间点
  auto endtime = std::chrono::system_clock::now();
  // 计算时间差
  auto delaytime = std::chrono::duration_cast<std::chrono::microseconds>(
      endtime - starttime);
  if (std::to_string(delaytime.count()).size() > 10)
  Logger::getInstance()->writeLogger(Logger::INFO, "start = " + message.timestr +
                                                       " end = " +
                                                       timeToStr(endtime));

  {
    std::unique_lock<std::mutex> lock(mutex_delay);
    DelayTime.push_back(std::to_string(delaytime.count()));
  }
  int closeFlag = 0;
  {
    std::unique_lock<std::mutex> lock(mutex_task);
    auto it = fd_tasks.find(fd);
    if (it == fd_tasks.end()) {
      std::cout << "fd_tasks中没有找到 in server recvTask" << std::endl;
      Logger::getInstance()->writeLogger(
          Logger::ERROR, "fd_tasks中没有找到 in server recvTask");
    }
    // std::cout << "数量 = " << it->second["number"] << std::endl;
    it->second["number"]--;
    if (it->second["isclose"] == 1) {
      closeFlag = 1;
      it->second["isclose"] = 0;
    }
  }
  if (closeFlag) {  // 删除MessageInfo中的消息
    std::cout << "线程中真正关闭" << std::endl;
    Logger::getInstance()->writeLogger(Logger::INFO,
                                       "线程中真正关闭 in server recvTask");
    {
      std::unique_lock<std::mutex> lock(mutex_combine);
      auto it = MessageInfo.find(fd);
      if (it == MessageInfo.end()) {
        std::cout << "删除连接时MessageInfo中找不到 in server recvTask"
                  << std::endl;
        Logger::getInstance()->writeLogger(
            Logger::ERROR, "删除连接时MessageInfo中找不到 in server recvTask");
      }
      MessageInfo.erase(it);
    }
    close(fd);
    // 计算平均时延，并打印
    long long int sum = 0;
    // 只统计后面一千个
    int count_sum = 0;
    for (auto s = DelayTime.rbegin(); s != DelayTime.rend(); s++) {
      if (count_sum > 1000) break;
      sum += std::stoi(*s);
      count_sum++;
    }
    std::string outputStr =
        "线程池中计算平均时延 = " +
        std::to_string(sum * 1.0 / (DelayTime.size() == 0 ? 1 : count_sum)) +
        " 收消息数量 = " + std::to_string(DelayTime.size());
    Logger::getInstance()->writeLogger(Logger::INFO, outputStr);
    std::cout << outputStr << std::endl;
  }
}

void Server::recvMessage() {
  struct sockaddr_in raddr;
  epollfd = epoll_create(2);
  if (epollfd < 0) {
    perror("epoll()");
    Logger::getInstance()->writeLogger(Logger::ERROR,
                                       "epoll error in server recvMessage");
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
  Logger::getInstance()->writeLogger(Logger::INFO,
                                     "服务器启动 in server recvMessage");

  while (true) {
    //
    my_int num = epoll_wait(epollfd, revents, REVENTSSIZE, -1);
    if (num < 0) {
      perror("epoll_wait()");
      Logger::getInstance()->writeLogger(
          Logger::ERROR, "epoll_wait error in server recvMessage");
      close(socket_d);
      close(epollfd);
      std::cout << "epoll_wait被中断的服务器关闭" << std::endl;
      Logger::getInstance()->writeLogger(
          Logger::INFO, "epoll_wait被中断的服务器关闭 in server recvMessage");
      int state = -1;
      pthread_exit(&state);
    }
    for (my_int i = 0; i < num; i++) {
      my_int fd = revents[i].data.fd;
      if (fd == socket_d) {
        // 表明服务器接收到了来自客户端的连接请求
        socklen_t len = sizeof(raddr);
        my_int newsd = accept(fd, (struct sockaddr *__restrict)&raddr, &len);
        /**
            @brief 测试代码
        */
        // int recv_buffer_size;
        // socklen_t optlen = sizeof(recv_buffer_size);
        // getsockopt(newsd, SOL_SOCKET, SO_RCVBUF, &recv_buffer_size, &optlen);
        // std::cout << "服务器接收缓冲区大小 = " << recv_buffer_size <<
        // std::endl;
        if (newsd < 0) {
          if (errno == EWOULDBLOCK) {
            // 表明本次通知的待处理事件全部处理完成
            break;
          }
          close(socket_d);
          close(epollfd);
          Logger::getInstance()->writeLogger(
              Logger::ERROR, "accept被中断的服务器关闭 in server recvMessage");
          perror("accept()");
          int state = -1;
          pthread_exit(&state);
        }
        char ip[16];
        inet_ntop(AF_INET, &raddr.sin_addr.s_addr, ip, sizeof(ip));
        char bp[100] = {'\0'};
        snprintf(bp, sizeof(bp), "client : %s, port : %d", ip,
                 ntohs(raddr.sin_port));
        Logger::getInstance()->writeLogger(Logger::INFO, bp);
        puts(bp);
        // 把socket_d套接口设置成非阻塞模式
        my_int flags = fcntl(newsd, F_GETFL, 0);
        fcntl(newsd, F_SETFL, flags | O_NONBLOCK);
        event.data.fd = newsd;
        event.events = EPOLLIN;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, newsd, &event);
        // 构建包消息
        std::vector<std::vector<std::string>> v;
        {
          std::unique_lock<std::mutex> lock(mutex_combine);
          MessageInfo.insert({newsd, v});
        }
        // 先查询fd_tasks中是不是已经存在fd
        {
          std::unique_lock<std::mutex> lock(mutex_task);
          // 表明fd_tasks中不存在fd，需要新建
          std::map<std::string, my_int> m;
          m["number"] = 1;
          m["state"] = 1;
          m["isclose"] = 0;
          fd_tasks[newsd] = m;
        }
      } else {
        // 表明是已经建立的连接要通信，而不是客户端请求连接
        char buf[BUFSIZE];
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
            Logger::getInstance()->writeLogger(
                Logger::ERROR, "持续读取错误 in server recvMessage");
            break;
          } else if (n == 0) {
            // perror("持续读取到结尾");
            std::cout << "持续读取到结尾 = " << len << std::endl;
            Logger::getInstance()->writeLogger(
                Logger::INFO, "持续读取到结尾 in server recvMessage");
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
          Logger::getInstance()->writeLogger(
              Logger::INFO, "正常的服务器关闭 in server recvMessage");
          // 清空
          DelayTime.clear();
          int state = 0;
          pthread_exit(&state);
        }
        if (len == 0) {
          // 表明客户端请求关闭
          puts("中继器请求关闭");
          Logger::getInstance()->writeLogger(
              Logger::INFO, "中继器请求关闭 in server recvMessage");
          auto it = fd_tasks.find(fd);
          if (it == fd_tasks.end()) {
            std::cout << "出现异常情况" << std::endl;
            Logger::getInstance()->writeLogger(
                Logger::ERROR, "出现异常情况 in server recvMessage");
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
                epoll_ctl(
                    epollfd, EPOLL_CTL_DEL, fd,
                    NULL);  // 把要监听的客户端套接字描述符从epoll实例中剔除
              }
            } else {
              // 可以在此线程中执行关闭
              Logger::getInstance()->writeLogger(
                  Logger::INFO, "epoll中真正关闭 in server recvMessage");
              it->second["state"] = 0;
              {
                std::unique_lock<std::mutex> lock(mutex_epollfd);
                epoll_ctl(
                    epollfd, EPOLL_CTL_DEL, fd,
                    NULL);  // 把要监听的客户端套接字描述符从epoll实例中剔除
              }
              close(fd);
              // 删除MessageInfo中的消息
              {
                std::unique_lock<std::mutex> lock(mutex_combine);
                auto it = MessageInfo.find(fd);
                if (it == MessageInfo.end()) {
                  std::cout << "MessageInfo中找不到" << std::endl;
                  Logger::getInstance()->writeLogger(
                      Logger::ERROR,
                      "MessageInfo中找不到 in server recvMessage");
                }
                MessageInfo.erase(it);
              }
              // 计算平均时延，并打印
              long long int sum = 0;
              // 只统计后面一千个
              int count_sum = 0;
              for (auto s = DelayTime.rbegin(); s != DelayTime.rend(); s++) {
                if (count_sum > 1000) break;
                std::cout << *s << std::endl;
                sum += std::stoi(*s);
                count_sum++;
              }
              std::string outputStr =
                  "主线程中计算平均时延 = " +
                  std::to_string(sum * 1.0 /
                                 (DelayTime.size() == 0 ? 1 : count_sum)) +
                  " 收消息数量 = " + std::to_string(DelayTime.size());
              Logger::getInstance()->writeLogger(Logger::INFO, outputStr);
              std::cout << outputStr << std::endl;
            }
          }
        } else if (len < 0) {
          std::cout << "epoll读取异常" << std::endl;
          Logger::getInstance()->writeLogger(
              Logger::ERROR, "epoll读取异常 in server recvMessage");
        } else {
          //消息处理，每个线程接收到的消息只是一个包，有些发送方的消息不只是只有一个包，所有线程之间要协作组合包
          // 1、先判断这个包是不是不需要和其他包组合
          Message message;
          deserializeStruct(buf, message);
          {
            if (message.packageSize == message.message.size()) {
              {
                std::unique_lock<std::mutex> lock(mutex_task);
                auto it = fd_tasks.find(fd);
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
              threadPool->enqueue([this, strs = message, tmpfd = fd]() {
                recvTask(strs, tmpfd);
              });
            } else {
              // 2、需要组合，再次判断是不是本次消息的最后一个包，如果是那么就要把所有包拿出来组合消息，然后打印到终端，
              // 如果不是最后一个包，那么就需要把包插入到合适的位置
              auto it = MessageInfo.find(fd);
              if (it == MessageInfo.end()) {
                std::cout << "MessageInfo中没找到" << std::endl;
                Logger::getInstance()->writeLogger(
                    Logger::ERROR, "MessageInfo中没找到 in server recvMessage");
              }
              int lent = 0;
              for (auto p : it->second) {
                lent += p[1].size();
              }
              lent += message.message.size();
              if (lent == message.packageSize) {
                // 表明是最后一个包
                // 组装包消息
                std::string pkgStrs = "";
                for (auto pkg : it->second) {
                  pkgStrs += pkg[1];
                }
                // 组装完成，打印消息
                pkgStrs += message.message;
                message.message = pkgStrs;
                {
                  std::unique_lock<std::mutex> lock(mutex_task);
                  auto it = fd_tasks.find(fd);
                  if (it == fd_tasks.end()) {
                    std::cout << "fd_tasks中没有找到" << std::endl;
                    Logger::getInstance()->writeLogger(
                        Logger::ERROR,
                        "fd_tasks中没有找到 in server recvMessage");
                  }
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
                threadPool->enqueue([this, strs = message, tmpfd = fd]() {
                  recvTask(strs, tmpfd);
                });
                // 删除MessageInfo中的包消息
                {
                  std::unique_lock<std::mutex> lock(mutex_combine);
                  it->second.clear();
                }
              } else {
                // 输出message
                std::vector<std::string> v;
                v.push_back(std::to_string(message.packageNum));
                v.push_back(message.message);
                if (it->second.size() == 0) {
                  // 直接插入
                  it->second.push_back(v);
                } else {
                  int f = 0;
                  std::vector<std::vector<std::string>>::iterator
                      index;  // 用于记录插入位置
                  for (auto pkg = it->second.begin(); pkg != it->second.end();
                       ++pkg) {
                    if (message.packageNum < std::stoi(*(pkg->begin()))) {
                      f = 1;
                      index = pkg;
                    }
                  }
                  if (f == 0) {
                    // 直接在末尾插入
                    it->second.push_back(v);
                  } else {
                    it->second.insert(index, v);
                  }
                }
              }
            }
          }
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
    @brief 服务器自己杀死这个分离的线程
*/
void Server::killSelf() {
  struct sockaddr_in raddr;
  raddr.sin_family = AF_INET;
  inet_pton(AF_INET, ip.c_str(), &raddr.sin_addr.s_addr);
  raddr.sin_port = htons(port);

  int socket_d = socket(AF_INET, SOCK_STREAM, 0);

  int val = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  if (connect(socket_d, (const struct sockaddr *)&raddr, sizeof(raddr)) < 0) {
    perror("connect()");
    Logger::getInstance()->writeLogger(Logger::ERROR,
                                       "connect error in server killSelf");
    close(socket_d);
    exit(-1);
  }
  char buf[BUFSIZE] = {'\0'};
  const char *str = "close\0";
  strcpy(buf, str);
  write(socket_d, buf, BUFSIZE);
  close(socket_d);
}