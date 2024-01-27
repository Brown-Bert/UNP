/**
 *
 * 使用epoll无论是实现异步还是同步都存在一定的优势以及问题
 * 同步用普通顺序函数实现
 * 异步使用线程池实现
 */

#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "mythreadspoll.h"

#define SERPORT 8888
#define POLLSIZE 1020
#define REVENTSSIZE 1024
#define FLAG false  // true表示使用线程进行异步通信 false同步通信

// 初始化线程池
ThreadsPoll *pollptr;

int main(int argc, char *argv[]) {
  int socket_fd;
  socklen_t val = 1;
  sockaddr_in laddr, raddr;
  pollptr = new ThreadsPoll;

#if FLAG
  pollptr->createPoll();
#endif

  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    perror("socket()");
    exit(-1);
  }

  // 设置地址可重用
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
    perror("setsockopt()");
    close(socket_fd);
    exit(-1);
  }

  // 作为服务器需要绑定本地地址
  laddr.sin_family = AF_INET;
  laddr.sin_port = htons(SERPORT);
  inet_pton(AF_INET, "0.0.0.0", &laddr.sin_addr.s_addr);
  if (bind(socket_fd, (const struct sockaddr *)&laddr, sizeof(laddr)) < 0) {
    perror("bind()");
    close(socket_fd);
    exit(-1);
  }

  // 设置监听池的大小
  if (listen(socket_fd, POLLSIZE) < 0) {
    perror("listen()");
    close(socket_fd);
    exit(-1);
  }

  // 把socket_fd套接口设置成非阻塞模式
  my_size_t flags = fcntl(socket_fd, F_GETFL, 0);
  fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);

  // 构造epoll实例
  my_size_t epollfd = epoll_create(2);
  if (epollfd < 0) {
    perror("epoll()");
    close(socket_fd);
    exit(-1);
  }

  // 创建事件结构体，把socket_fd放进去
  struct epoll_event event;
  event.data.fd = socket_fd;
  event.events = EPOLLIN | EPOLLET;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, socket_fd, &event);

  // 定义返回事件集合
  struct epoll_event revents[REVENTSSIZE];
  while (true) {
    my_size_t revents_num = epoll_wait(epollfd, revents, REVENTSSIZE, -1);
    if (revents_num < 0) {
      perror("epoll_wait()");
      //可以想办法遍历所有注册时间的描述符，然后一个一个close，也可以等待程序退出然后系统关闭所有打开的描述符
      exit(-1);
    }
    for (my_size_t i = 0; i < revents_num; i++) {
      my_size_t fd = revents[i].data.fd;
      if (fd == socket_fd) {
        // 表明有客户端请求连接
        while (true) {
          /**
          因为设置的是边沿模式所以只是通知一次，
          所以要把等待处理的等待连接事件全部处理完成，
          因此需要把监听套接字设置成非阻塞模式
          */
          socklen_t len = sizeof(raddr);
          my_size_t newfd =
              accept(socket_fd, (struct sockaddr *__restrict)&raddr,
                     (socklen_t *__restrict)&len);
          if (newfd < 0) {
            if (errno == EWOULDBLOCK) {
              // 表明本次通知的待处理事件全部处理完成
              break;
            }
            perror("accept()");
            exit(-1);
          }
          // // 把newfd套接口设置成阻塞模式
          my_size_t f = fcntl(newfd, F_GETFL, 0);
          // f &= ~O_NONBLOCK;
          fcntl(newfd, F_SETFL, f | O_NONBLOCK);

          char ip[16];
          inet_ntop(AF_INET, &raddr.sin_addr.s_addr, ip, sizeof(ip));
          // printf("client : %s, port : %d\n", ip, ntohs(raddr.sin_port));
          event.data.fd = newfd;
          event.events = EPOLLIN | EPOLLET;
          epoll_ctl(epollfd, EPOLL_CTL_ADD, newfd, &event);
        }
      } else {
        // 客户端和服务器进行通信
#if FLAG  // 使用异步，交给线程处理
        pollptr->enqueue(fd);
#else  // 使用同步
        pollptr->dealTask(fd);
#endif
      }
    }
  }
  close(socket_fd);
  close(epollfd);
  exit(0);
}
