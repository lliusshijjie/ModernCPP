// 第 23 课示例：std::scoped_lock 与 shared_mutex（C++17）
//
// 结构：① 原理验证 —— CTAD 推导；多锁一起锁/逆序放；不可拷贝
//       ② 语法演示 —— shared_mutex 读写协议（第 06 课的轻量版）
//       ③ 应用实战 —— 双向并发转账不死锁；双读者并行计时
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 23_scoped_lock.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

// ③ 并发转账的账户：每账户一把锁
struct Account {
    std::mutex m;
    long balance = 0;
};

// scoped_lock 一次锁两把 —— 死锁免疫（内置 std::lock 的 try/back-off）
bool transfer(Account& from, Account& to, long amount) {
    std::scoped_lock lk(from.m, to.m);          // CTAD → scoped_lock<mutex, mutex>
    if (from.balance < amount) return false;
    from.balance -= amount;
    to.balance += amount;
    return true;
}

// ③ 读多写少缓存：shared_mutex（C++17 轻量版，协议同第 06 课）
class Cache {
public:
    std::string get(const std::string& k) const {
        std::shared_lock<std::shared_mutex> r(mtx_);   // 读：共享
        auto it = data_.find(k);
        return it == data_.end() ? "" : it->second;
    }
    void set(const std::string& k, std::string v) {
        std::unique_lock<std::shared_mutex> w(mtx_);   // 写：独占
        data_[k] = std::move(v);
    }
private:
    mutable std::shared_mutex mtx_;              // const 接口加锁 → mutable
    std::map<std::string, std::string> data_;
};

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：CTAD —— 传两把锁自动推出 scoped_lock<mutex, mutex>
    std::mutex m1, m2;
    {
        std::scoped_lock lk(m1, m2);
        static_assert(std::is_same_v<decltype(lk),
                                     std::scoped_lock<std::mutex, std::mutex>>,
                      "CTAD works");
        // 两把都已持有：从外部 try 是拿不到的（本线程再 try 会死锁，不演示）
    }
    // 验证 2：离开作用域后全部释放 —— 两把都能 try_lock 成功
    std::cout << m1.try_lock() << ' ' << m2.try_lock() << '\n';   // true true
    m1.unlock();
    m2.unlock();

    // 验证 3：不可拷贝不可移动（防"转手所有权"破坏 RAII 配对）
    static_assert(!std::is_copy_constructible_v<std::scoped_lock<std::mutex>> &&
                  !std::is_move_constructible_v<std::scoped_lock<std::mutex>>,
                  "scoped_lock is fixed to its scope");

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 shared_mutex：读者与读者并存（协议同第 06 课，实现更轻）
    std::shared_mutex sm;
    {
        std::shared_lock<std::shared_mutex> r1(sm);
        std::shared_lock<std::shared_mutex> r2(sm, std::defer_lock);
        std::cout << r2.try_lock() << '\n';              // true —— 读读共存
    }

    // 2.2 写者压制读者
    {
        std::unique_lock<std::shared_mutex> w(sm);
        std::shared_lock<std::shared_mutex> r(sm, std::defer_lock);
        std::cout << r.try_lock() << '\n';               // false —— 写者持有
    }

    // 2.3 单锁的 scoped_lock：等价 lock_guard 的零开销默认选择
    {
        std::scoped_lock single(m1);
    }
    std::cout << m1.try_lock() << '\n';                  // true —— 已释放
    m1.unlock();

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 双向并发转账：A->B 与 B->A 各 1000 笔，每笔 1 元
    //     余额给足（每笔必然成功）→ 无论怎样交错，双向净额都恰为 0，
    //     结果确定；裸写两个 lock_guard 迟早死锁，scoped_lock 免疫
    //     （若余额只给 100，耗尽后转账失败，最终分布就随交错而变 —— 非确定）
    Account A, B;
    A.balance = 1000000;
    B.balance = 1000000;
    auto worker = [](Account& x, Account& y) {
        for (int i = 0; i < 1000; ++i) transfer(x, y, 1);
    };
    std::thread t1(worker, std::ref(A), std::ref(B));    // A -> B 1000 次
    std::thread t2(worker, std::ref(B), std::ref(A));    // B -> A 1000 次
    t1.join();
    t2.join();
    std::cout << A.balance << ' ' << B.balance << ' '
              << (A.balance + B.balance) << '\n';   // 1000000 1000000 2000000

    // 3.2 双读者并行计时：各持 shared_lock 睡 150ms
    //      并行 -> 总耗时 < 250ms；若读被串行化（写锁/普通锁）则 >= 300ms
    Cache cache;
    cache.set("k", "v");
    const auto hold = std::chrono::milliseconds(150);
    std::shared_mutex probe;
    auto t0 = std::chrono::steady_clock::now();
    std::thread r1([&] {
        std::shared_lock<std::shared_mutex> r(probe);
        std::this_thread::sleep_for(hold);
    });
    std::thread r2([&] {
        std::shared_lock<std::shared_mutex> r(probe);
        std::this_thread::sleep_for(hold);
    });
    r1.join();
    r2.join();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    std::cout << (ms < 250) << '\n';                     // true —— 读者真并行
    std::cout << cache.get("k") << '\n';                 // v

    return 0;
}
