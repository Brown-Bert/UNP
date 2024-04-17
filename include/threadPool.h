#ifndef THREADPOOL_H_
#define THREADPOOL_H_
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

class ThreadPool {
 public:
  ThreadPool(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
      workers.emplace_back([this] {
        while (true) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [this] { return stop || !tasks.empty(); });
            if (stop && tasks.empty()) {
              return;
            }
            task = std::move(tasks.front());
            tasks.pop();
          }
          // std::cout << "任务数量 = " << tasks.size() << std::endl;
          task();
        }
      });
    }
  }

  template <typename Func, typename... Args>
  void enqueue(Func&& func, Args&&... args) {
    {
      std::unique_lock<std::mutex> lock(mutex);
      tasks.emplace([func, &args...]() mutable {
        (std::forward<Func>(func))(std::forward<Args>(args)...);
      });
    }
    condition.notify_one();
  }

  void setNumThreads(size_t numThreads) {
    if (numThreads == 0) {
      // 不允许将线程数设置为0
      return;
    }

    size_t currentNumThreads = workers.size();
    if (numThreads > currentNumThreads) {
      // 增加线程数量
      for (size_t i = currentNumThreads; i < numThreads; ++i) {
        workers.emplace_back([this] {
          while (true) {
            std::function<void()> task;

            {
              std::unique_lock<std::mutex> lock(mutex);
              condition.wait(lock, [this] { return stop || !tasks.empty(); });

              if (stop && tasks.empty()) {
                return;
              }

              task = std::move(tasks.front());
              tasks.pop();
            }

            task();
          }
        });
      }
    } else if (numThreads < currentNumThreads) {
      // 减少线程数量
      {
        std::unique_lock<std::mutex> lock(mutex);
        stop = true;
      }
      condition.notify_all();

      for (std::thread& worker : workers) {
        worker.join();
      }

      workers.resize(numThreads);
      stop = false;

      for (size_t i = currentNumThreads; i < numThreads; ++i) {
        workers[i] = std::thread([this] {
          while (true) {
            std::function<void()> task;

            {
              std::unique_lock<std::mutex> lock(mutex);
              condition.wait(lock, [this] { return stop || !tasks.empty(); });

              if (stop && tasks.empty()) {
                return;
              }

              task = std::move(tasks.front());
              tasks.pop();
            }

            task();
          }
        });
      }
    }
  }

  ~ThreadPool() {
    {
      std::unique_lock<std::mutex> lock(mutex);
      stop = true;
    }
    condition.notify_all();
    for (std::thread& worker : workers) {
      worker.join();
    }
  }

 private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;
  std::mutex mutex;
  std::condition_variable condition;
  bool stop;
};
#endif