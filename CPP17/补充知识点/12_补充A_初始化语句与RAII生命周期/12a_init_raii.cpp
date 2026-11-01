// 补充 12A：初始化语句与 RAII 生命周期（C++17）。
// ① 原理验证 -> ② 语法演示 -> ③ 应用实战；输出仅使用 ASCII。
// 从 ModernCPP 目录编译：
// g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread "CPP17/补充知识点/12_补充A_初始化语句与RAII生命周期/12a_init_raii.cpp" -o run

#include <iostream>
#include <mutex>
#include <stdexcept>

struct ScopeProbe {
    int& alive;
    explicit ScopeProbe(int& count) : alive(count) { ++alive; }
    ~ScopeProbe() { --alive; }
    ScopeProbe(const ScopeProbe&) = delete;
    ScopeProbe& operator=(const ScopeProbe&) = delete;
};

void add_score(std::mutex& mutex, int& score, bool allowed) {
    // 条件为 false 也已经获得锁；else 执行期间仍持有锁。
    if (std::unique_lock<std::mutex> lock(mutex); allowed) {
        score += 5;
        std::cout << "updated " << lock.owns_lock() << ' ' << score << '\n'; // updated true 5
    } else {
        std::cout << "denied " << lock.owns_lock() << ' ' << score << '\n'; // denied true 0
    }
    // lock 在整个 if/else 结束后析构并解锁；这里不再持有锁。
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";
    int alive = 0;
    if (ScopeProbe guard(alive); false) {
        std::cout << "unreachable\n";
    } else {
        std::cout << "else " << alive << '\n'; // else 1
    }
    std::cout << "after-if " << alive << '\n'; // after-if 0

    std::cout << "===== 2. SYNTAX =====\n";
    int evaluated = 0;
    switch (ScopeProbe guard(alive); ++evaluated) {
    case 1:
        std::cout << "case " << alive << ' ' << evaluated << '\n'; // case 1 1
        break; // 退出 switch，guard 随后析构。
    default:
        break;
    }
    std::cout << "after-switch " << alive << '\n'; // after-switch 0
    try {
        if (ScopeProbe guard(alive); true) throw std::runtime_error("demo");
    } catch (const std::runtime_error&) {
        std::cout << "after-throw " << alive << '\n'; // after-throw 0
    }

    std::cout << "===== 3. APPLICATION =====\n";
    std::mutex mutex;
    int score = 0;
    add_score(mutex, score, false); // denied true 0
    add_score(mutex, score, true);  // updated true 5；前一次调用的锁已释放。
    {
        std::lock_guard<std::mutex> check(mutex);
        std::cout << "final-score " << score << '\n'; // final-score 5
    }
    if (alive != 0 || evaluated != 1 || score != 5) return 1;
    // 不在持锁线程中再次 try_lock 验证锁状态；不要对非递归 mutex 递归加锁。
}
