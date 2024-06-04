// #include <fcntl.h>
// #include <unistd.h>

// #include <chrono>
// #include <csignal>
// #include <iostream>
// #include <random>
// #include <string>
// #include <thread>
// #include <vector>

// #include "repeater.h"
// #include "threadPool.h"

// bool CLIENTSTOP = false;

// void signalHandler(int signal) {
//   // 释放中继服务器的线程池
//   CLIENTSTOP = true;
// }

// std::string ConstructStringWithByteSize(std::size_t size) {
//   std::vector<char> charVector(size - 1, 'a');
//   return std::string(charVector.begin(), charVector.end()) + '\0';
// }

// int main(int argc, char* argv[]) {
//   // 注册信号
//   struct sigaction sa;
//   sa.sa_handler = signalHandler;  // 设置信号处理函数
//   sigemptyset(&sa.sa_mask);       // 清空信号屏蔽字
//   sa.sa_flags = 0;                // 设置默认标志

//   // 注册信号处理程序
//   if (sigaction(SIGINT, &sa, NULL) == -1) {
//     std::cerr << "Failed to register signal handler" << std::endl;
//     exit(-1);
//   } else {
//     std::cout << "信号注册成功" << std::endl;
//   }
//   // 构造1000个客户端给100个服务器发送消息
//   if (argc < 4) {
//     std::cerr << "参数有问题" << std::endl;
//     return 0;
//   }
//   my_int startPort = serverPortStart;
//   int clientNum = std::stoi(argv[1]);
//   int byteSize = std::stoi(argv[2]);   // 传输的字节大小，单位值B
//   int threadNum = std::stoi(argv[3]);  // 线程池的线程数量
//   // 输出
//   std::cout << "clientNum: " << clientNum << std::endl;
//   std::cout << "byteSize: " << byteSize << std::endl;
//   std::vector<Client> clients;
//   {  // 放在域中，为了出域的时候调用线程池的析构函数
//     // 创建线程池
//     // ThreadPool threadPool(threadNum);
//     std::string str = ConstructStringWithByteSize(byteSize);
//     for (int i = 0; i < clientNum; i++) {
//       if (startPort > (serverPortStart + serverNum - 1)) {
//         startPort = serverPortStart;
//       }
//       Client client(i);
//       client.createSocket();
//       // sleep(2);
//       my_int flags = fcntl(client.socket_d, F_GETFL, 0);
//       fcntl(client.socket_d, F_SETFL, flags | O_NONBLOCK);
//       client.setIpAndPort();
//       client.server_ip = serverIp;
//       client.server_port = startPort;
//       client.msg = str;
//       clients.push_back(client);
//       // while (!CLIENTSTOP) {
//       //   client.sendMessage(serverIp, startPort, str);
//       //   // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//       // }

//       // std::cout << i << std::endl;
//       // std::this_thread::sleep_for(std::chrono::milliseconds(100));
//       // sleep(2);
//       std::cout << "客户端数量 = " << clients.size() << std::endl;
//       startPort++;
//     }
//     for (auto t = clients.begin(); t != clients.end(); ++t) {
//       std::thread thread_tmp([&t](){
//         if (CLIENTSTOP) {
//           return;
//         }
//         t->sendMessage();
//       });
//       thread_tmp.detach();
//     }
//     // while (!CLIENTSTOP) {
//     //   // 发送消息
//     //   for (auto t = clients.begin(); t != clients.end(); ++t) {
//     //     // threadPool.enqueue([p = t](){
//     //     //   {
//     //     //     // std::unique_lock<std::mutex> lock(t.mtx);
//     //     //     p->sendMessage();
//     //     //   }
//     //     // });
//     //     t->sendMessage();
//     //     // std::this_thread::sleep_for(std::chrono::milliseconds(50));
//     //   }
//     // }
//   }
//   while(!CLIENTSTOP) sleep(2);
//   for (auto& t : clients) {
//     t.closefd();
//   }

//   std::cout << "结束" << std::endl;
//   return 0;
// }

#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <csignal>

#include "repeater.h"
#include "threadPool.h"

bool CLIENTSTOP = false;

void signalHandler(int signal) {
    // 释放中继服务器的线程池
    CLIENTSTOP = true;
}

std::string ConstructStringWithByteSize(std::size_t size) {
  std::vector<char> charVector(size - 1, 'a');
  return std::string(charVector.begin(), charVector.end()) + '\0';
}

int main(int argc, char* argv[]) {
  // 注册信号
  struct sigaction sa;
  sa.sa_handler = signalHandler; // 设置信号处理函数
  sigemptyset(&sa.sa_mask);      // 清空信号屏蔽字
  sa.sa_flags = 0;               // 设置默认标志

  // 注册信号处理程序
  if (sigaction(SIGINT, &sa, NULL) == -1) {
      std::cerr << "Failed to register signal handler" << std::endl;
      exit(-1);
  }else {
    std::cout << "信号注册成功" << std::endl;
  }
  // 构造1000个客户端给100个服务器发送消息
  if (argc < 4) {
    std::cerr << "参数有问题" << std::endl;
    return 0;
  }
  my_int startPort = serverPortStart;
  int clientNum = std::stoi(argv[1]);
  int byteSize = std::stoi(argv[2]);  // 传输的字节大小，单位值B
  int threadNum = std::stoi(argv[3]); // 线程池的线程数量
  // 输出
  std::cout << "clientNum: " << clientNum << std::endl;
  std::cout << "byteSize: " << byteSize << std::endl;
  std::vector<std::thread> threadsHandler;
  {
    std::string str = ConstructStringWithByteSize(byteSize);
    for (int i = 0; i < clientNum; i++) {
      if (startPort > (serverPortStart + serverNum - 1)) {
        startPort = serverPortStart;
      }
      threadsHandler.emplace_back([=](){
        Client client(i);
        client.createSocket();
        // sleep(2);
        my_int flags = fcntl(client.socket_d, F_GETFL, 0);
        fcntl(client.socket_d, F_SETFL, flags | O_NONBLOCK);
        client.setIpAndPort();
        client.server_ip = serverIp;
        client.server_port = startPort;
        client.msg = str;
        while (!CLIENTSTOP) {
          client.sendMessage();
        }
        client.closefd();
      });
      // while (!CLIENTSTOP) {
      //   client.sendMessage(serverIp, startPort, str);
      //   // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      // }

      // std::cout << i << std::endl;
      // std::this_thread::sleep_for(std::chrono::milliseconds(100));
      // sleep(2);
      std::cout << "客户端数量 = " << threadsHandler.size() << std::endl;
      startPort++;
    }
  }
  for (auto& t : threadsHandler) {
    std::cout << "join" << std::endl;
    t.join();
  }

  std::cout << "结束" << std::endl;
  return 0;
}