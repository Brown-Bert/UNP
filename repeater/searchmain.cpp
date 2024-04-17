#include <iostream>

#include "repeater.h"

int main(int argc, char* argv[]) {
  // 创建一个进程获取中继服务器中的会话信息,
  // 实际开发中只需要单独开一个线程就行，减少资源消耗
  searchClient();
  return 0;
}