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
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

// ------ 最小生成器框架：读懂它 = 读懂协程的一半 ------
template <typename T>
class Gen {
public:
    struct promise_type {                       // ① 编译器找 promise 的暗号
        std::optional<T> current_;
        std::exception_ptr error_;

        Gen get_return_object() {               // 协程入口创建返回对象
            return Gen{Handle::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept {   // ② 惰性启动
            return {};
        }
        std::suspend_always final_suspend() noexcept {     // ③ 终态也挂起：
            return {};                                    //    保住协程帧可查 done
        }
        std::suspend_always yield_value(T v) {  // ④ co_yield 的落点
            current_.emplace(std::move(v));
            return {};
        }
        void return_void() {}                   // ⑤ co_return 的收尾
        void unhandled_exception() noexcept {   // ⑥ 转存异常供 next() 重抛
            error_ = std::current_exception();
        }
    };

    using Handle = std::coroutine_handle<promise_type>;

    explicit Gen(Handle h) : h_(h) {}
    Gen(Gen&& o) noexcept : h_(std::exchange(o.h_, {})) {}
    Gen(const Gen&) = delete;
    Gen& operator=(const Gen&) = delete;
    ~Gen() { if (h_) h_.destroy(); }            // RAII：销毁协程帧

    bool done() const { return !h_ || h_.done(); } // 空句柄视为已结束
    std::optional<T> next() {                    // 产出一个值，结束则 nullopt
        if (done()) return std::nullopt;         // 禁止恢复空句柄或终态协程
        auto& p = h_.promise();
        p.current_.reset();                     // 完成时不返回上一次 yield 的值
        h_.resume();
        if (p.error_) std::rethrow_exception(p.error_);
        return std::move(p.current_);
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
        co_yield a;                             // 机器整数有界，不允许有符号溢出
        if (a > std::numeric_limits<long long>::max() - b) {
            co_yield b;                         // 最后一项仍可表示，然后正常结束
            co_return;
        }
        auto next = a + b;                      // 状态保存在协程帧里
        a = b;
        b = next;
    }
}

int main() {
    std::cout << std::boolalpha;
    auto check = [](bool condition) {
        if (!condition) throw std::logic_error("generator boundary check failed");
    };
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：惰性启动——initial_suspend 挂起，创建后函数体一行未跑
    auto g = trace(2);
    std::cout << "main: created (body not started)\n";
    // main: created（coro 行未出现）

    // 验证 2：resume/coroutine body 交替执行——两台发动机轮流开火
    auto first = g.next();
    std::cout << "main: take " << first.value() << '\n';
    auto second = g.next();
    std::cout << "main: take " << second.value() << '\n';
    // 顺序：body starts / before yield 1 / take 1 / resumed / before yield 2 / take 2

    // 验证 3：最后一个 yield 后再 resume → 走完 co_return，done 为真
    std::cout << "main: done=" << g.done() << '\n';    // false（还停在 yield 2 处）
    auto end = g.next();                              // resumed /（协程退出）
    check(!end);
    check(!g.next());                               // 再次读取终态仍安全
    std::cout << "main: done=" << g.done() << '\n';    // true

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 六件套速览（对照上方 Gen 框架编号）：
    //   get_return_object / initial_suspend / final_suspend
    //   yield_value / return_void / unhandled_exception
    // 2.2 可能挂起的位置：initial_suspend、co_await/co_yield、final_suspend
    //     co_return 结束函数体并进入最终挂起协议；co_await 也可能不挂起
    // 2.3 suspend_always vs suspend_never（"eager 发动机"）：
    //   initial_suspend 返回 suspend_never 时，创建即执行到首个挂起点——
    //   适合"任务"语义；生成器语义（按需拉取）选 suspend_always

    // 2.4 句柄操作清单：resume() 恢复 / done() 查终态 / destroy() 销毁帧
    //    框架里由 Gen 的 RAII 托管 destroy——手调 destroy 是高级用法

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 惰性斐波那契：帧里冻结 a/b，取到哪算到哪
    auto f = fib();
    for (int i = 0; i < 8; ++i) std::cout << f.next().value() << ' ';
    std::cout << '\n';                          // 0 1 1 2 3 5 8 13

    // 3.2 早停：不取了就析构——destroy 只销毁已挂起的帧，安全
    {
        auto f2 = fib();
        std::cout << f2.next().value() << ' ' << f2.next().value() << '\n'; // 0 1
    }                                           // f2 析构：帧被销毁

    // 3.3 边界验证：移走后的空句柄、异常传递、有界整数的正常耗尽
    auto original = fib();
    auto moved = std::move(original);
    check(original.done() && !original.next());
    check(moved.next().value() == 0);

    auto failing = []() -> Gen<int> {
        co_yield 7;
        throw std::runtime_error("generator failed");
    };
    auto bad = failing();
    check(bad.next().value() == 7);
    bool caught = false;
    try {
        (void)bad.next();
    } catch (const std::runtime_error&) {
        caught = true;
    }
    check(caught && bad.done() && !bad.next());

    auto bounded = fib();
    long long last = 0;
    while (auto value = bounded.next()) last = *value;
    if constexpr (std::numeric_limits<long long>::digits == 63)
        check(last == 7540113804746346429LL);
    else
        check(last > 0);
    std::cout << "boundary checks: passed\n";

    // 3.4 实战心法（对照 ①②③）：
    //   co_yield 的每次"交还"都是重新调度点——生成器里别持锁跨 yield
    //   promise 是协程与调用方之间的"信箱"（current_）
    //   想要现成轮子：cppcoro / std::generator 等 C++23（第 54 课）
    return 0;
}
