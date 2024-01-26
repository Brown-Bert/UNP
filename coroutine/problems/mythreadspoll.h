/**
    自己写一个线程库
*/
#ifndef MYTHREADSPOLL_H_
#define MYTHREADSPOLL_H_

#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#define my_size_t int
#define DEFAULTNUM 4
#define TASKSMAXSIZE \
  1020  // 一个进程所拥有的文件描述符列表（一般系统资源中默认打开的文件时1024）
#define BUFSIZE 1024

class ThreadsPoll {
 private:
  // 任务队列，用于存放任务（就是套接字描述符，服务器负责接收把任务放进队列，然后通知线程中的某个线程去任务队列中拿任务执行和客户端的通信）
  std::queue<my_size_t> tasks;

  // 任务队列中最大容量
  my_size_t tasksMaxSize = TASKSMAXSIZE;

  // 多线程环境下，多个线程去操作和修改任务队列，那么任务队列就是临界资源
  std::mutex mtx;

  // 服务器要通知线程池去拿任务，就需要条件变量
  std::condition_variable cond;

  // 线程池中线程的个数
  my_size_t threadsNum = DEFAULTNUM;

  // 线程池
  std::vector<std::thread> poll;

  // 结束标志 true表示暂停， false表示继续
  bool stop;

  // 记录当前线程池的个数，用于析构函数中结束线程
  my_size_t currentSize = 0;

  // currentSize是多线程共享的变量需要加锁
  std::mutex mtt;

  // 起始时间
  decltype(std::chrono::high_resolution_clock::now()) start_time;

  // 结束时间
  decltype(std::chrono::high_resolution_clock::now()) end_time;

 public:
  ThreadsPoll() : stop(false){};
  ThreadsPoll(my_size_t num) : stop(false) { threadsNum = num; }
  void createPoll() {
    currentSize = threadsNum;
    for (int i = 0; i < threadsNum; i++) {
      poll.emplace_back([this] {
        while (1) {
          my_size_t fd;
          {
            std::unique_lock<std::mutex> lock(mtx);
            cond.wait(
                lock,
                [this] {  // lambda结果是true表明等待的条件成立，继续向下执行
                  return stop || !tasks.empty();
                });
            if (stop && tasks.empty()) {
              {
                std::unique_lock<std::mutex> lo(mtt);
                currentSize--;
              }
              return;
            }
            // 从任务队列中拿出套接字描述符
            fd = tasks.front();
            tasks.pop();
          }
          dealTask(fd);
        }
      });
    }
  }
  // 处理客户端和服务器具体通信
  void dealTask(my_size_t fd) {
    // 和客户端进行通信
    char buf[BUFSIZE];
    my_size_t n = read(fd, buf, BUFSIZE);
    if (n == 0) {
      // 表明收到了对方的EOF，也就是客户端关闭了
      close(fd);
      return;
    }
    std::string num(buf);
    std::chrono::milliseconds dura(
        std::stoi(num));  // 休眠的时间是num毫秒
                          // 用休眠时间去模拟客户端和服务器通信的时间
    if (std::stoi(num) == 1) {
      // 开始计时
      // 获取当前时间点
      start_time = std::chrono::high_resolution_clock::now();
    } else if (std::stoi(num) == 0) {
      // 结束计时
      end_time = std::chrono::high_resolution_clock::now();
      // 计算时间差
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
          end_time - start_time);

      // 输出耗时（以微秒为单位）
      std::cout << "Elapsed time: " << duration.count() << " microseconds"
                << std::endl;
    }
    std::this_thread::sleep_for(dura);
  }
  void enqueue(my_size_t socket_fd) {
    {
      std::unique_lock<std::mutex> lock(mtx);
      tasks.emplace(socket_fd);
    }
    // 通知线程池中的一个线程去任务队列中拿任务（有可能所有线程都在忙，就会导致这个通知信号，没有一个线程去执行）
    cond.notify_one();
  }
  ~ThreadsPoll() {
    stop =
        true;  // 虽然多个线程要用到stop，但是不需要加锁，因为线程只是查看值，并没有修改，唯一修改的是构造函数和析构函数在修改
    // 执行析构函数的时候任务队列大概率是还有任务的
    while (!tasks.empty() || currentSize) {
      cond.notify_one();
    }
    /**
     * 唤醒所有线程用于结束线程被唤醒的线程会多次尝试还是只是尝试一次获取锁，获取不到就睡眠，等待下一次通知，
     * 之所以所有线程能正常终止，是因为每个线程判断结束条件，处理结束操作的时间比较短，释放锁之后其他线程还在竞争并没有睡眠。
     * 但是也有概率出现处理的时间较长，其他竞争的线程就去睡眠，等待下一次通知，所以这个方法不保险，解决方法：直接去观察线程池
     * 如果池中还有线程那么就去通知线程
     */
    // cond.notify_all();

    // 为每个线程收尸
    for (std::thread &t : poll) {
      t.join();
      std::cout << "终止线程" << std::endl;
    }
  }
};

#endif