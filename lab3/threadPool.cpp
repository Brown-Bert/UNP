#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadPool {
public:
    ThreadPool(size_t numThreads) : stop(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mutex);
                        condition.wait(lock, [this] {
                            return stop || !tasks.empty();
                        });
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

    template <typename Func, typename... Args>
    void enqueue(Func&& func, Args&&... args) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            tasks.emplace([func, args...] {
                func(args...);
            });
        }
        condition.notify_one();
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
std::mutex mtx;
// 示例任务函数
void taskFunction(int num) {
    std::unique_lock<std::mutex> ll(mtx);
    std::cout << "Task ---" << num << "--- is running in thread " << std::this_thread::get_id() << std::endl;
}

int main() {
    ThreadPool threadPool(4);  // 创建一个拥有 4 个线程的线程池

    // 提交任务到线程池
    for (int i = 0; i < 8; ++i) {
        threadPool.enqueue(taskFunction, i);
    }

    // 等待任务完成
    std::this_thread::sleep_for(std::chrono::seconds(2));

    return 0;
}