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

ThreadsPoll *pollptr;

int main() {
  // my_size_t socket_fd;
  // socklen_t val = 1;
  // sockaddr_in laddr, raddr, addr;

  // socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  // if (socket_fd < 0) {
  //   perror("socket()");
  //   exit(-1);
  // }

  // // 设置地址可重用
  // if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0)
  // {
  //   perror("setsockopt()");
  //   close(socket_fd);
  //   exit(-1);
  // }

  // // 作为服务器需要绑定本地地址
  // laddr.sin_family = AF_INET;
  // laddr.sin_port = htons(8888);
  // inet_pton(AF_INET, "127.0.0.1", &laddr.sin_addr.s_addr);
  // if (bind(socket_fd, (const struct sockaddr *)&laddr, sizeof(laddr)) < 0) {
  //   perror("bind()");
  //   close(socket_fd);
  //   exit(-1);
  // }

  // // 设置监听池的大小
  // if (listen(socket_fd, 5) < 0) {
  //   perror("listen()");
  //   close(socket_fd);
  //   exit(-1);
  // }

  // socklen_t len = sizeof(raddr);
  // my_size_t newfd = accept(socket_fd, (struct sockaddr *__restrict)&raddr,
  //                          (socklen_t *__restrict)&len);
  // if (newfd < 0) {
  //   perror("accept()");
  //   exit(-1);
  // }
  // // 获取本地地址
  // socklen_t locallen = sizeof(addr);
  // getsockname(newfd, (struct sockaddr *__restrict)&addr, &locallen);

  // char ip[16];
  // inet_ntop(AF_INET, &addr.sin_addr.s_addr, ip, sizeof(ip));
  // printf("client : %s, port : %d\n", ip, ntohs(addr.sin_port));
  // char buf[1024];
  // sleep(1);
  // read(newfd, buf, 1024);
  // printf("%c", buf[2]);
  {
    pollptr = new ThreadsPoll;
    pollptr->createPoll();
    pollptr->enqueue(1);
    pollptr->enqueue(22);
    delete pollptr;
  }
  sleep(5);
  exit(0);
}