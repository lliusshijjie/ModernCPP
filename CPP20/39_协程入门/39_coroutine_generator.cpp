// 第 39 课示例：协程 coroutine 入门（C++20）
//
// 结构：① 原理验证 —— 挂起/恢复的执行顺序可视化；两台"发动机"
//       ② 语法演示 —— 手写最小生成器：promise 六件套 + RAII 句柄
//       ③ 应用实战 —— 惰性斐波那契数列生成器
//
// 编译运行：
//   g++ -std=c++20 -Wall -Wextra 39_coroutine_generator.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <coroutine>
#include <iostream>
#include <utility>

// ------ 最小生成器框架：读懂它 = 读懂协程的一半 ------
template <typename T>
class Gen {
public:
    struct promise_type {                       // ① 编译器找 promise 的暗号
        T current_{};

        Gen get_return_object() {               // 协程入口创建返回对象
            return Gen{Handle::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept {   // ② 惰性启动
            return {};
        }
        std::suspend_always final_suspend() noexcept {     // ③ 终态也挂起：
            return {};                                    //    保住栈帧可查 done
        }
        std::suspend_always yield_value(T v) {  // ④ co_yield 的落点
            current_ = v;
            return {};
        }
        void return_void() {}                   // ⑤ co_return 的收尾
        void unhandled_exception() {}           // ⑥ 异常（入门版吞掉）
    };

    using Handle = std::coroutine_handle<promise_type>;

    explicit Gen(Handle h) : h_(h) {}
    Gen(Gen&& o) noexcept : h_(std::exchange(o.h_, {})) {}
    Gen(const Gen&) = delete;
    Gen& operator=(const Gen&) = delete;
    ~Gen() { if (h_) h_.destroy(); }            // RAII：销毁协程帧

    bool done() const { return h_.done(); }     // 跑到 final_suspend 了吗
    T next() {                                  // 恢复执行到下一个挂起点
        h_.resume();
        return h_.promise().current_;
    }

private:
    Handle h_;
};

// ① 执行顺序可视化：打印版生成器（演示"两台发动机"轮流开火）
Gen<int> trace(int n) {
    std::cout << "coro: body starts\n";
    for (int i = 1; i <= n; ++i) {
        std::cout << "coro: before yield " << i << '\n';
        co_yield i;                             // 此处挂起，控制权交还调用者
        std::cout << "coro: resumed\n";
    }
    co_return;                                  // 走 return_void + final_suspend
}

// ③ 实战：惰性斐波那契——取多少算多少，状态"冻"在帧里
Gen<long long> fib() {
    long long a = 0, b = 1;
    while (true) {
        co_yield a;                             // 无穷序列：永不到 final
        auto next = a + b;                      // 状态冻结在协程帧里
        a = b;
        b = next;
    }
}

int main() {
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：惰性启动——initial_suspend 挂起，创建后函数体一行未跑
    auto g = trace(2);
    std::cout << "main: created (body not started)\n";
    // main: created（coro 行未出现）

    // 验证 2：resume/coroutine body 交替执行——两台发动机轮流开火
    std::cout << "main: take " << g.next() << '\n';
    std::cout << "main: take " << g.next() << '\n';
    // 顺序：body starts / before yield 1 / take 1 / resumed / before yield 2 / take 2

    // 验证 3：最后一个 yield 后再 resume → 走完 co_return，done 为真
    std::cout << "main: done=" << g.done() << '\n';    // false（还停在 yield 2 处）
    (void)g.next();                                    // resumed /（协程退出）
    std::cout << "main: done=" << g.done() << '\n';    // true

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 六件套速览（对照上方 Gen 框架编号）：
    //   get_return_object / initial_suspend / final_suspend
    //   yield_value / return_void / unhandled_exception
    // 2.2 挂起点只有三类：co_await / co_yield / co_return
    // 2.3 suspend_always vs suspend_never（"eager 发动机"）：
    //   initial_suspend 返回 suspend_never 时，创建即执行到首个挂起点——
    //   适合"任务"语义；生成器语义（按需拉取）选 suspend_always

    // 2.4 句柄操作清单：resume() 恢复 / done() 查终态 / destroy() 销毁帧
    //    框架里由 Gen 的 RAII 托管 destroy——手调 destroy 是高级用法

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 惰性斐波那契：帧里冻结 a/b，取到哪算到哪
    auto f = fib();
    for (int i = 0; i < 8; ++i) std::cout << f.next() << ' ';
    std::cout << '\n';                          // 0 1 1 2 3 5 8 13

    // 3.2 早停：不取了就析构——destroy 只销毁已挂起的帧，安全
    {
        auto f2 = fib();
        std::cout << f2.next() << ' ' << f2.next() << '\n';   // 0 1
    }                                           // f2 析构：帧被销毁

    // 3.3 实战心法（对照 ①②③）：
    //   co_yield 的每次"交还"都是重新调度点——生成器里别持锁跨 yield
    //   promise 是协程与调用方之间的"信箱"（current_）
    //   想要现成轮子：cppcoro / std::generator 等 C++23（第 54 课）
    return 0;
}
