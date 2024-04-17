#include <iostream>

#include "repeater.h"

int main(int argc, char* argv[]) {
  // 创建中继器
  RelayServer relayServer;
  // 创建线程池
  relayServer.createPool(100);
  relayServer.createSocket(SERVERIP, SERVERPORT, 1);
  relayServer.recvMsg();
  return 0;
}