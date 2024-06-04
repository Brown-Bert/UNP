#include "client.h"
#include "log.h"

void Client::createSocket() {
  struct sockaddr_in raddr;
  raddr.sin_family = AF_INET;
  inet_pton(AF_INET, SERVERIP, &raddr.sin_addr.s_addr);
  raddr.sin_port = htons(SERVERPORT);

  socket_d = socket(AF_INET, SOCK_STREAM, 0);
  my_int val = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  if (connect(socket_d, (const struct sockaddr *)&raddr, sizeof(raddr)) < 0) {
    perror("connect()");
    close(socket_d);
    exit(-1);
  }
  /**
    * @brief 获取客户端接收缓冲区大小
    * 测试代码
  */
  // int send_buffer_size;
  // socklen_t optlen = sizeof(send_buffer_size);
  // getsockopt(socket_d, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, &optlen);
  // std::cout << "客户端发送缓冲区大小 = " << send_buffer_size << std::endl;
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