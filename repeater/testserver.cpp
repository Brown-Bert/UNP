
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <iostream>

int main() {
  struct sockaddr_in laddr;
  laddr.sin_family = AF_INET;
  inet_pton(AF_INET, "127.0.0.1", &laddr.sin_addr.s_addr);
  laddr.sin_port = htons(6666);

  int socket_d = socket(AF_INET, SOCK_STREAM, 0);

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
  // 使用epoll
  struct sockaddr_in raddr;
  int epollfd = epoll_create(2);
  if (epollfd < 0) {
    perror("epoll()");
    exit(-1);
  }
  struct epoll_event event;
  event.data.fd = socket_d;
  event.events = EPOLLIN;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, socket_d, &event);

  struct epoll_event revents[1024];
  while (true) {
    int num = epoll_wait(epollfd, revents, 1024, -1);
    if (num < 0) {
      perror("epoll_wait()");
      exit(-1);
    }
    for (int i = 0; i < num; i++) {
      int fd = revents[i].data.fd;
      if (fd == socket_d) {
        // 表明服务器接收到了来自客户端的连接请求
        // while (true) {
        socklen_t len = sizeof(raddr);
        int newsd = accept(socket_d, (struct sockaddr *__restrict)&raddr, &len);
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
        // printf("client : %s, port : %d\n", ip, ntohs(raddr.sin_port));
        if (newsd < 0) {
          perror("accept()");
          close(socket_d);
          exit(-1);
        }

        // 把socket_d套接口设置成非阻塞模式
        int flags = fcntl(newsd, F_GETFL, 0);
        fcntl(newsd, F_SETFL, flags | O_NONBLOCK);
        event.data.fd = newsd;
        event.events = EPOLLIN | EPOLLET;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, newsd, &event);
        std::cout << "jinru" << std::endl;
        // }
      } else {
        char buf[1024];
        while (true) {
          int len = read(fd, buf, 1024);
          std::cout << "len = " << len << " errno = " << errno << std::endl;
          if (len < 0) break;
        }
      }
    }
  }
  return 0;
}