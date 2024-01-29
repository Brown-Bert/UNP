/**
    自己写一个线程库
*/
#ifndef MYTHREADSPOLL_H_
#define MYTHREADSPOLL_H_

#include <asm-generic/errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#define my_size_t int
#define DEFAULTNUM 4
#define TASKSMAXSIZE \
  604857  // 一个进程所拥有的文件描述符列表（一般早期系统资源中默认打开的文件时1024，现代拥有更高）
#define BUFSIZE 1024
class ThreadsPoll;
extern ThreadsPoll *pollptr;

typedef struct {
  void *esp;
  void *ebp;
  void *eip;
  void *edi;
  void *esi;
  void *ebx;
  void *r1;
  void *r2;
  void *r3;
  void *r4;
  void *r5;
} context;

struct coroutine {
  my_size_t fd;
  context ctx;         // 上下文
  void *func(void *);  // 协程入口函数
  void *param;         // 入口函数的参数

  void *stack;           // 协程的栈指针
  my_size_t stack_size;  // 栈大小

  unsigned int status;  // 状态 ready running defer wait
  // queue_node(coroutinr) ready_node;
  // rbtree_node(coroutine) defer_node;
  // rbtree_node(coroutine) wait_node;
};

class ThreadsPoll {
 private:
  // 任务队列，用于存放任务（就是套接字描述符，服务器负责接收把任务放进队列，然后通知线程中的某个线程去任务队列中拿任务执行和客户端的通信）
  std::queue<my_size_t> tasks;

  // 任务队列中最大容量
  my_size_t tasksMaxSize = TASKSMAXSIZE;

  // 当前任务数量
  my_size_t currentTasksSize = 0;

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
  std::mutex mtp;

  // 任务队列有容量并不是不设上限，当达到上线之后使用条件变量进行阻塞
  std::condition_variable condition;

  // 起始时间
  decltype(std::chrono::high_resolution_clock::now()) start_time;

  // 结束时间
  decltype(std::chrono::high_resolution_clock::now()) end_time;

  // 关闭线程的辅助标志
  my_size_t closeThreadFlag = 0;

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
              // 退出线程之前，要把它拥有的锁全部释放
              lock.unlock();
              return;
            }
            // 从任务队列中拿出套接字描述符
            fd = tasks.front();
            tasks.pop();
            currentTasksSize--;
            if (currentTasksSize == (tasksMaxSize - 1)) {
              // std::cout << "通知" << currentTasksSize << std::endl;
              condition.notify_one();
            }
          }
          // std::cout << "执行任务" << std::endl;
          dealTask(fd);
        }
      });
    }
  }
  // 处理客户端和服务器具体通信
  // 因为epoll是一轮一轮处理的，客户端不停地发送分节，大概率这些分节会处于不同的epoll轮次中
  // 所以肯定会造成多个线程去管理一个描述符字，如果此时描述符设置为阻塞模式那么可能发生管理
  // 该描述符的一个线程的read处于阻塞，另一个管理该描述符的线程收到对端关闭的通知，那么此时第一个线程
  // 的read就会因为关闭描述字被唤醒，然后去操作描述字读取数据，但此时描述字被关闭，所以read返回-1且
  // errno 设置为EBADF/* Bad file number
  // */。所以可以把描述字设置成非阻塞模式，但此时read返回的0和-1
  // 所设定的情况就会有所重叠，不能通过0和-1来判断了，就需要对端发送自定义的结束标志
  void dealTask(my_size_t fd) {
    // std::cout << "fd = " << fd << std::endl;

    // 测试描述字是不是阻塞
    // int flags = fcntl(fd, F_GETFL);
    // if (flags & O_NONBLOCK) {
    //   // 描述符为非阻塞模式
    //   std::cout << "非阻塞" << std::endl;
    // } else {
    //   // 描述符为阻塞模式
    //   std::cout << "阻塞" << std::endl;
    // }
    // 和客户端进行通信
    while (true) {  // 持续读，可能本次套接口缓冲区的数据比BUFSIZE还多
      char buf[BUFSIZE] = {0};
      my_size_t n = read(fd, buf, BUFSIZE);
      // std::cout << "n = " << n << std::endl;
      if (n <= 0) {
        // 本轮处理中并没有收到对方的请求关闭的通知，但是本轮数据没有了也需要跳出循环
        // std::cout << "errno = " << errno << std::endl;
        if (errno == EWOULDBLOCK) {
          // std::cout << "没有数据" << std::endl;
          errno = 0;
        } else if (errno == 9) {
          // std::cout << "操作一个已经关闭或本身就不存在的描述字" << std::endl;
          errno = 0;
        }
        // epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL); //
        // 删除epoll实例对其的监测
        close(fd);
        break;
      }

      // buf中大概率存在这样的数据{'1', '\0', '2', '\0', '3', '\0'....}
      my_size_t index = 0;  // 用于指示buf的下标
      my_size_t pos = 0;    // 每一个子串的初始位置
      my_size_t breakFlag = 0;
      char subBuf[BUFSIZE];
      while (buf[index] != '\0') {
        index++;
        if (buf[index] == '\0') {
          strncpy(subBuf, buf + pos, index + 2 - pos);
          pos = index + 1;
          index = pos;
          std::string num(subBuf);
          // std::cout << "num = " << num << std::endl;
          std::chrono::milliseconds dura(
              std::stoi(num));  // 休眠的时间是num毫秒
          // 用休眠时间去模拟客户端和服务器通信的时间
          // std::cout << "sdgf = " << std::stoi(num) << std::endl;
          if (std::stoi(num) == 2) {
            // 对端自定义本次通信结束标志，此时可以关闭描述字
            close(fd);
            // std::cout << "flag ========= " << closeThreadFlag << std::endl;
            {
              std::unique_lock<std::mutex> lock(mtp);
              if (closeThreadFlag) {
                closeThreadFlag = 0;
                std::thread([] {
                  delete pollptr;
                  return;
                }).detach();
              }
            }
            breakFlag = 1;
            break;
          }
          if (std::stoi(num) == 3) {
            // 通过客户端辅助来优雅关闭线程
            std::cout << "关闭线程池" << std::endl;
            closeThreadFlag = 1;
          }
          if (std::stoi(num) == 1) {
            // 开始计时
            // 获取当前时间点
            // std::cout << "开始测试时间" << std::endl;
            start_time = std::chrono::high_resolution_clock::now();
            // break;
          } else if (std::stoi(num) == 0) {
            // 结束计时
            end_time = std::chrono::high_resolution_clock::now();
            // 计算时间差
            auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    end_time - start_time);

            // 输出耗时（以微秒为单位）
            std::cout << "Elapsed time: " << duration.count() << " microseconds"
                      << std::endl;
            // std::cout << "结束测试时间" << std::endl;
            // break;
          } else {
            std::this_thread::sleep_for(dura);
          }
        }
      }
      if (breakFlag) break;
    }
  }
  void enqueue(my_size_t socket_fd) {
    {
      std::unique_lock<std::mutex> lock(mtx);
      if (currentTasksSize == tasksMaxSize) {
        // std::cout << "size = " << currentTasksSize << std::endl;
        condition.wait(lock, [this] {
          // std::cout << "收到通知-=-----" << currentTasksSize << std::endl;
          return (currentTasksSize <
                  tasksMaxSize);  // 任务队列能放任务就继续执行，否则阻塞在此
        });
        // std::cout << "sizedsf = " << currentTasksSize << std::endl;
      }
      tasks.emplace(socket_fd);
      currentTasksSize++;
      // std::cout << "size123 = " << currentTasksSize << std::endl;
    }
    // 通知线程池中的一个线程去任务队列中拿任务（有可能所有线程都在忙，就会导致这个通知信号，没有一个线程去执行）
    cond.notify_one();
  }
  ~ThreadsPoll() {
    stop =
        true;  // 虽然多个线程要用到stop，但是不需要加锁，因为线程只是查看值，并没有修改，唯一修改的是构造函数和析构函数在修改
    // 执行析构函数的时候任务队列大概率是还有任务的
    while (!tasks.empty() || currentSize) {
      // tasks.pop();
      // std::cout << "emp = " << tasks.empty() << ", size = " << currentSize
      //           << std::endl;
      // sleep(1);
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
      // std::cout << "终止线程" << std::endl;
    }
    // closeT.join();
    exit(0);
  }
};

#endif