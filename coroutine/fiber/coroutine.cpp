// fiber.cpp
#include "coroutine.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>

static std::atomic<uint64_t> gFiberId(0);     // 协程ID
static std::atomic<uint64_t> gFiberCount(0);  // 当前协程总数

static thread_local Coroutine *gCurrFiber = nullptr;  // 当前正在执行的协程
static thread_local Coroutine::sp gThreadMainFiber =
    nullptr;  // 一个线程的主协程

// TODO: 配置yaml文件，获取栈大小
uint64_t getStackSize() {
  static uint64_t size = 1024 * 1024;
  if (size == 0) {
    return -1;
  }
  return size;
}

class MallocAllocator {
 public:
  static void *alloc(uint64_t size) { return malloc(size); }
  static void dealloc(void *ptr, uint64_t size) { free(ptr); }
};

using Allocator = MallocAllocator;

Coroutine::Coroutine() : mFiberId(++gFiberId) {
  ++gFiberCount;
  mState = EXEC;
  if (getcontext(&mCtx)) {
    perror("getcontext()");
    exit(-1);
  }
  SetThis(this);
}

Coroutine::Coroutine(std::function<void()> cb, uint64_t stackSize)
    : mFiberId(++gFiberId), mCb(cb), mState(READY) {
  ++gFiberCount;

  mStackSize = stackSize ? stackSize : getStackSize();
  mStack = Allocator::alloc(mStackSize);
  if (getcontext(&mCtx)) {
    perror("getcontext()");
    exit(-1);
  }
  mCtx.uc_stack.ss_sp = mStack;
  mCtx.uc_stack.ss_size = mStackSize;
  mCtx.uc_link = nullptr;
  makecontext(&mCtx, &FiberEntry, 0);
}

Coroutine::~Coroutine() {
  --gFiberCount;
  if (mStack) {
    Allocator::dealloc(mStack, mStackSize);
  } else {  // main fiber
    if (gCurrFiber == this) {
      SetThis(nullptr);
    }
  }
}

// 调用位置在主协程中。
void Coroutine::Reset(std::function<void()> cb) {
  mCb = cb;
  if (getcontext(&mCtx)) {
    perror("getcontext()");
    exit(-1);
  }
  mCtx.uc_stack.ss_sp = mStack;
  mCtx.uc_stack.ss_size = mStackSize;
  mCtx.uc_link = nullptr;
  makecontext(&mCtx, &FiberEntry, 0);
  mState = READY;
}

/**
 * 同一个线程应使SwapIn的调用次数比Yeild调用次数多一，如果多二则会在FiberEntry结尾处退出线程
 * 如果不多一则会使只能指针的引用计数大于1，导致释放不干净，原因是FiberEntry的回调未执行完毕，
 * 即最后一次Yeild操作保存了堆栈，使得在栈上的只能无法释放
 */
void Coroutine::SwapIn() {
  SetThis(this);
  mState = EXEC;
  if (swapcontext(&gThreadMainFiber->mCtx, &mCtx)) {
    perror("swapcontext()");
    exit(-1);
  }
}

void Coroutine::SwapOut() {
  SetThis(gThreadMainFiber.get());
  if (swapcontext(&mCtx, &gThreadMainFiber->mCtx)) {
    perror("swapcontext()");
    exit(-1);
  }
}

void Coroutine::SetThis(Coroutine *f) { gCurrFiber = f; }

Coroutine::sp Coroutine::GetThis() {
  if (gCurrFiber) {
    return gCurrFiber->shared_from_this();
  }
  Coroutine::sp fiber(new Coroutine());
  gThreadMainFiber = fiber;
  return gCurrFiber->shared_from_this();
}

void Coroutine::Resume() { SwapIn(); }

void Coroutine::Yeild2Hold() {
  Coroutine::sp ptr = GetThis();
  ptr->mState = HOLD;
  ptr->SwapOut();
}

void Coroutine::Yeild2Ready() {
  Coroutine::sp ptr = GetThis();
  ptr->mState = READY;
  ptr->SwapOut();
}

void Coroutine::FiberEntry() {
  Coroutine::sp curr = GetThis();
  try {
    curr->mCb();
    curr->mCb = nullptr;
    curr->mState = TERM;
  } catch (const std::exception &e) {
    curr->mState = EXCEPT;
  } catch (...) {
    curr->mState = EXCEPT;
  }

  Coroutine *ptr = curr.get();
  curr.reset();
  ptr->SwapOut();
}
