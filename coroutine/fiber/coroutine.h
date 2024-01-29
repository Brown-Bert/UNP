// fiber.h
#ifndef __COROUTINE_H__
#define __COROUTINE_H__

#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>

#include <functional>
#include <memory>

#define UNKNOWN_ERROR -1;
/**
 * 协程类
 */
class Coroutine : public std::enable_shared_from_this<Coroutine> {
 public:
  typedef std::shared_ptr<Coroutine> sp;
  enum FiberState {
    HOLD,   // 暂停状态
    EXEC,   // 执行状态
    TERM,   // 结束状态
    READY,  // 可执行态
    EXCEPT  // 异常状态
  };
  Coroutine();
  Coroutine(std::function<void()> cb, uint64_t stackSize = 0);
  ~Coroutine();

  void Reset(std::function<void()> cb);
  static void SetThis(Coroutine *f);  // 设置当前正在执行的协程
  static Coroutine::sp GetThis();     // 获取当前正在执行的协程
  void Resume();                      // 唤醒协程
  static void
  Yeild2Hold();  // 将当前正在执行的协程让出执行权给主协程，并设置状态为HOLD

 private:
  static void
  Yeild2Ready();  // 将当前正在执行的协程让出执行权给主协程，并设置状态为READY
  static void FiberEntry();  // 协程入口函数
  void SwapIn();             // 切换到前台, 获取执行权限
  void SwapOut();            // 切换到后台, 让出执行权限

 private:
  ucontext_t mCtx;      // 存储每个协程对象自己的上下文信息
  FiberState mState;    // 当前协程的状态
  uint64_t mFiberId;    // 协程号
  uint64_t mStackSize;  // 每个协程都有自己的栈
  void *mStack;         // 协程自己的栈指针
  std::function<void()>
      mCb;  // 虽然可以定义每个协程不同的入口函数，
            // 但是为了统一，可以定义一样的入口函数，在内部调用不一样的实际函数
};
#endif  // __FIBER_H__
