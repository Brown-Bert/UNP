#include "client.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include "log.hpp"
#include "utils.hpp"

/**
  @brief 客户端创建套接字的封装函数
*/

void Client::createSocket() {
  struct sockaddr_in raddr;
  raddr.sin_family = AF_INET;
  inet_pton(AF_INET, SERVERIP, &raddr.sin_addr.s_addr);
  raddr.sin_port = htons(SERVERPORT);

  socket_d = socket(AF_INET, SOCK_STREAM, 0);
  my_int val = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  if (connect(socket_d, (const struct sockaddr *)&raddr, sizeof(raddr)) < 0) {
    Logger::getInstance()->writeLogger(Logger::ERROR,
                                       "connect error in client createSocket");
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

void Client::closefd() { close(socket_d); }

/**
  @brief
  如果采用默认不绑定本地端口，那么建立连接时会自动分配，将分配的端口记录下来
*/
void Client::setIpAndPort() {
  struct sockaddr_in laddr;
  socklen_t len = sizeof(laddr);
  if (getsockname(socket_d, (struct sockaddr *)&laddr, &len) < 0) {
    Logger::getInstance()->writeLogger(
        Logger::ERROR, "getsocketname error in client setIpAndPort");
    close(socket_d);
    exit(-1);
  }
  char ipt[16];
  inet_ntop(AF_INET, &laddr.sin_addr.s_addr, ipt, sizeof(ipt));
  ip = ipt;
  port = laddr.sin_port;
}

/**
  @brief 客户端发送消息
*/
void Client::sendMessage() {
  // 如果消息够长，则会造成缓冲区溢出，而且自己定义的数据结构在接收方并不能按照预计的那样去读取，
  // 所以需要将具体的消息手动切片或者补齐
  // 发送信息
  auto tmpstr = msg;
  Message message;
  message.sourceIp = ip;
  message.sourcePort = port;
  message.desIp = server_ip;
  message.desPort = server_port;
  message.packageSize = tmpstr.size();
  // 获取当前时间点
  auto currentTimePoint = std::chrono::system_clock::now();
  message.timestr = timeToStr(currentTimePoint);
  int LEN =
      BUFSIZE - (sizeof(my_int) + ip.size() + sizeof(port) + sizeof(my_int) +
                 server_ip.size() + sizeof(server_port) +
                 sizeof(message.packageSize) + sizeof(message.packageNum) +
                 sizeof(my_int) + message.timestr.size() + sizeof(my_int));
  int count = 0;
  {
    while (true) {
      if (tmpstr.size() > LEN) {
        message.packageNum = count;
        count++;
        message.message = tmpstr.substr(0, LEN);
        tmpstr = tmpstr.substr(LEN);
        // 将结构体序列化
        char buf[BUFSIZE] = {'\0'};
        serializeStruct(message, buf);
        errno = 0;
        my_int len = 0;
        while (len < BUFSIZE) {
          my_int n = write(socket_d, buf + len, BUFSIZE - len);
          if (n < 0) {
            if (errno == EWOULDBLOCK || EAGAIN) {
              if (len == 0) return;
              Logger::getInstance()->writeLogger(Logger::WARNING, "缓冲区满了");
              continue;
            } else {
              Logger::getInstance()->writeLogger(
                  Logger::ERROR, "write error in client sendMessage");
              exit(-1);
            }
          }
          len += n;
        }
      } else {
        message.message = tmpstr;
        message.packageNum = count;
        // 将结构体序列化
        char buf[BUFSIZE] = {'\0'};
        serializeStruct(message, buf);
        errno = 0;
        my_int len = 0;
        while (len < BUFSIZE) {
          my_int n = write(socket_d, buf + len, BUFSIZE - len);
          if (n < 0) {
            if (errno == EWOULDBLOCK || EAGAIN) {
              Logger::getInstance()->writeLogger(Logger::WARNING, "缓冲区满了");
              continue;
            } else {
              Logger::getInstance()->writeLogger(
                  Logger::ERROR, "write error in client sendMessage");
              exit(-1);
            }
          }
          len += n;
        }
        break;
      }
    }
  }
}