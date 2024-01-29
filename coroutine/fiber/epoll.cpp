// epoll.cpp
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
// #include <log/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
// #include <utils/Errors.h>
// #include <utils/thread.h>

#include "coroutine.h"

#define LOG_TAG "epoll"
#define EPOLL_EVENT_SIZE 512
#define RD_BUF_SIZE 512
#define WR_BUF_SIZE 512

int InitSocket(uint16_t port = 8080) {
  int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    // LOGE("socket error. error code = %d, error message: %s", errno,
    //      strerror(errno));
    return UNKNOWN_ERROR;
  }
  sockaddr_in server_addr;
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  ;

  int opt = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  if (::bind(sock, (sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    // LOGE("bind error. error code = %d, error message: %s", errno,
    //      strerror(errno));
    goto error_return;
  }
  if (::listen(sock, 128) < 0) {
    // LOGE("listen error. error code = %d, error message: %s", errno,
    //      strerror(errno));
    goto error_return;
  }
  return sock;

error_return:
  ::close(sock);
  return UNKNOWN_ERROR;
}

static int gServerSocket = 0;
static int gEpollFd = 0;

void AcceptClient() {
  if (!gServerSocket || !gEpollFd) {
    return;
  }
  static epoll_event event;
  sockaddr_in client_addr;
  bzero(&client_addr, sizeof(client_addr));
  socklen_t addrLen = 0;
  int clientSock = 0;
  clientSock = ::accept(gServerSocket, (sockaddr *)&client_addr, &addrLen);
  if (clientSock < 0) {
    return;
  }
  // 设置非阻塞
  int flags = fcntl(clientSock, F_GETFL, 0);
  fcntl(clientSock, F_SETFL, flags | O_NONBLOCK);
  event.data.fd = clientSock;
  event.events = EPOLLIN | EPOLLET;
  epoll_ctl(gEpollFd, EPOLL_CTL_ADD, clientSock, &event);
}

static epoll_event gEvent;
static int gClientFd = 0;
void ReadFormClient() {
  if (gClientFd <= 0) {
    return;
  }
  char readBuf[RD_BUF_SIZE] = {0};
  bzero(readBuf, RD_BUF_SIZE);
  int readSize = ::read(gClientFd, readBuf, RD_BUF_SIZE);
  if (readSize < 0) {
    if (errno == ECONNRESET) {
      epoll_ctl(gEpollFd, EPOLL_CTL_DEL, gClientFd, &gEvent);
      close(gClientFd);
    }
  } else if (readSize == 0) {
    epoll_ctl(gEpollFd, EPOLL_CTL_DEL, gClientFd, &gEvent);
    close(gClientFd);
  } else {
    // 进行通信
  }
  gClientFd = 0;
}

int main(int argc, char *argv[]) {
  int serverSock = InitSocket();
  if (serverSock < 0) {
    return 0;
  }

  char readBuf[RD_BUF_SIZE] = {0};
  int allSock[EPOLL_EVENT_SIZE] = {0};
  int clientSock = -1;
  sockaddr_in client_addr;
  bzero(&client_addr, sizeof(client_addr));
  socklen_t addrLen = 0;

  struct epoll_event event;
  struct epoll_event allEvent[EPOLL_EVENT_SIZE];

  int epollfd = epoll_create(EPOLL_EVENT_SIZE);
  event.events = EPOLLIN | EPOLLET;  // 边沿触发，数据到达才会触发
  event.data.fd = serverSock;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, serverSock, &event);
  gServerSocket = serverSock;
  gEpollFd = epollfd;
  Coroutine::GetThis();
  Coroutine::sp acceptFiebr(new Coroutine(AcceptClient));
  Coroutine::sp readFiber(new Coroutine(ReadFormClient));
  while (1) {
    int ret = epoll_wait(epollfd, allEvent, EPOLL_EVENT_SIZE, -1);
    if (ret < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    } else if (ret > 0) {
      for (int i = 0; i < ret; ++i) {
        int fdtmp = allEvent[i].data.fd;
        gClientFd = fdtmp;
        if (fdtmp == serverSock) {  // accept event
          acceptFiebr->Resume();
          acceptFiebr->Reset(AcceptClient);
        } else if (allEvent[i].events | EPOLLIN) {  // read event
          readFiber->Resume();
          readFiber->Reset(ReadFormClient);
        }
      }
    }
  }
  exit(0);
}
