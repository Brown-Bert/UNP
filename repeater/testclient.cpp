
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>

int main() {
  struct sockaddr_in raddr;
  raddr.sin_family = AF_INET;
  inet_pton(AF_INET, "127.0.0.1", &raddr.sin_addr.s_addr);
  raddr.sin_port = htons(6666);

  int socket_d = socket(AF_INET, SOCK_STREAM, 0);

  int val = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  // setsockopt(socket_d, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));

  if (connect(socket_d, (const struct sockaddr *)&raddr, sizeof(raddr)) < 0) {
    perror("connect()");
    exit(-1);
  }
  //   write(socket_d, "123\0", 4);
  int f;
  while (true) {
    std::cin >> f;
    if (f == 0) break;
    write(socket_d, "123\0", 4);
    close(socket_d);
  }
  //   sleep(1);
  close(socket_d);
  return 0;
}