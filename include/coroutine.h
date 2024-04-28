// #ifndef __COROUTINE_H__
// #define __COROUTINE_H__
// #include <coroutine>
// #include <cstdlib>
// #include <iostream>
// #include <optional>
// #include <utility>
// template <typename T>
// struct MyCoroGenerator {
//   /**
//       @brief
//      C++协程要求Generate必须promise_type这个类型，名字也必须是promise_type
//       最方便的方法就是定义在Generator内部
//   */
//   struct promise_type {
//     /**
//         @brief 存储协程的返回值
//         optional库：
//         1、表示可能缺失的值：std::optional
//        允许你在某些情况下表示一个值的存在或缺失。你可以将一个值包装在
//        std::optional 对象中，如果该值存在，则可以通过解引用 std::optional
//        对象来访问该值；如果该值不存在，则可以采取适当的处理逻辑。

//         2、避免空指针问题：传统的方式中，使用指针来表示可能的缺失值，但这容易出现空指针异常。std::optional
//        提供了一种更安全的替代方案，避免了空指针问题。

//         3、增加可读性和易用性：使用 std::optional
//        可以提高代码的可读性和易用性。通过明确地使用 std::optional
//        来表示一个值的存在或缺失，可以更清晰地表达代码的意图和预期行为。

//         4、减少错误处理逻辑：有时，一个函数可能返回一个特殊值来表示错误或无效的结果。使用
//        std::optional 可以减少错误处理逻辑，因为你可以直接返回一个
//        std::optional 对象，而不需要使用特殊值或错误码。
//     */
//     std::optional<T> opt;

//     /**
//         @brief 协程创建的时候就必须挂起，函数名字也必须是initial_suspend
//         suspen_always、suspend_never是标准库中定义好的类型，前者表示总是挂起，后者表示从不挂起
//         struct suspend_always
//         {
//             constexpr bool await_ready() const noexcept { return false; }

//             constexpr void await_suspend(coroutine_handle<>) const noexcept
//             {}

//             constexpr void await_resume() const noexcept {}
//         };

//         struct suspend_never
//         {
//             constexpr bool await_ready() const noexcept { return true; }

//             constexpr void await_suspend(coroutine_handle<>) const noexcept
//             {}

//             constexpr void await_resume() const noexcept {}
//         };
//         @return std::suspend_always
//     */
//     std::suspend_always initial_suspend() const {
//       return {};  //
//       suspend_always是一个结构体，return返回 {}
//       表明用列表初始化suspend_always
//     }

//     /**
//         @brief 携程最后一次执行完成之后必须挂起，函数名字必须是final_suspend
//         由于final_suspend是收尾阶段的工作，因此必须是noexcept，表明不会抛出异常
//         @return std::suspend_always
//     */
//     std::suspend_always final_suspend() const noexcept { return {}; }

//     /**
//         @brief 处理协程中未捕获的异常，函数名字必须是unhandled_exception
//     */
//     void unhandled_exception() { std::exit(EXIT_FAILURE); }

//     /**
//         @brief 获取一个Generator对象，该对象从promise_type构造
//         @return MyCoroGenerator
//     */
//     MyCoroGenerator get_return_object() {
//       return MyCoroGenerator{
//           std::coroutine_handle<promise_type>::from_promise(*this)};
//     }

//     /**
//         @brief 定制yield_value接口，接收co_yield返回的值
//         @tparam Arg 值类型
//         @param arg co_yield返回的值
//         @return std::suspend_always 执行完之后继续挂起
//     */
//     template <typename Arg>
//     std::suspend_always yield_value(Arg&& arg) {
//       opt.emplace(std::forward<Arg>(arg));
//       return {};
//     }

//     /**
//         @brief 当协程结束co_return并且没有返回值时，调用该函数
//         还有一个return_value(expr)函数，处理有返回值的情况
//     */
//     void return_value() {}
//   };

//   /**
//     @brief 协程句柄，存储协程的上下文信息，包裹在MyCoroGenerator
//   */
//   std::coroutine_handle<promise_type> handle;

//   /**
//     @brief 通过handle构造一个Generator
//     @param h 从promise_type构造出来协程的句柄
//   */
//   MyCoroGenerator(std::coroutine_handle<promise_type> h) : handle(h) {}

//   /**
//     默认构造函数
//   */
//   MyCoroGenerator(){};

//   /**
//     @brief 移动构造函数
//     @param other 其他Generator对象
//   */
//   MyCoroGenerator(MyCoroGenerator&& other) {
//     if (handle) {
//       handle.destroy();
//     }
//     handle = other.handle;
//     other.handle = nullptr;
//   }

//   /**
//     移动赋值函数
//     @param other 其他的Generator对象
//     @return MyCoroGenerator
//   */
//   MyCoroGenerator& operator=(MyCoroGenerator&& other) {
//     if (handle) {
//       handle.destroy();
//     }
//     handle = other.handle;
//     other.handle = nullptr;
//     return *this;
//   }

//   /**
//     @brief 析构函数
//   */
//   ~MyCoroGenerator() {
//     if (handle) handle.destroy();
//   }

//   /**
//     @brief 继续执行协程，并返回执行结果
//     @return T&
//   */
//   T& next() {
//     handle.resume();
//     if (handle.done()) {
//       throw "Generator Error";
//     }
//     return *(handle.promise().opt);
//   }

//  private:
//   MyCoroGenerator(const MyCoroGenerator&) = delete;
//   MyCoroGenerator& operator=(const MyCoroGenerator&) = delete;
// };
// #endif  // __FIBER_H__
