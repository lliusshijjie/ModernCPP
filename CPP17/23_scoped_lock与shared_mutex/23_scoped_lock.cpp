// 第 23 课示例：std::scoped_lock 与 shared_mutex（C++17）
//
// 结构：① 原理验证 —— CTAD 推导；多锁统一管理；不可拷贝
//       ② 语法演示 —— shared_mutex 读写协议（第 06 课的轻量版）
//       ③ 应用实战 —— 双向并发转账不死锁；双读者并行计时
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 23_scoped_lock.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <chrono>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// ③ 并发转账的账户：每账户一把锁
struct Account {
    std::mutex m;
    long balance = 0;
};

// scoped_lock 协调两把锁，规避这次组合加锁的死锁；具体算法由实现选择。
bool transfer(Account& from, Account& to, long amount) {
    if (&from == &to || amount < 0) return false; // 本接口拒绝自转账和负数金额。
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
        // 两把都已持有；不能让本线程再次 try_lock 这两把非递归锁。
    }
    // 验证 2：离开作用域后全部释放，可以再次取得所有权。
    {
        std::unique_lock first(m1);
        std::unique_lock second(m2);
        std::cout << first.owns_lock() << ' ' << second.owns_lock() << '\n'; // true true
    }

    // 验证 3：不可拷贝不可移动（防"转手所有权"破坏 RAII 配对）
    static_assert(!std::is_copy_constructible_v<std::scoped_lock<std::mutex>> &&
                  !std::is_move_constructible_v<std::scoped_lock<std::mutex>>,
                  "scoped_lock is fixed to its scope");

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 shared_mutex：读者与读者并存（协议同第 06 课，实现更轻）
    std::shared_mutex sm;
    {
        std::shared_lock<std::shared_mutex> r1(sm);
        auto reader = std::async(std::launch::async, [&sm] {
            std::shared_lock r2(sm); // 必须由另一个线程验证，不能递归持有。
            return r2.owns_lock();
        });
        std::cout << reader.get() << '\n';              // true —— 两线程读读共存
    }

    // 2.2 写者压制读者
    {
        std::unique_lock<std::shared_mutex> w(sm);
        auto reader = std::async(std::launch::async, [&sm] {
            std::shared_lock r(sm, std::try_to_lock);
            return r.owns_lock();
        });
        std::cout << reader.get() << '\n';               // false —— 另一线程持有写锁
    }

    // 2.3 单锁的 scoped_lock：等价 lock_guard 的零开销默认选择
    {
        std::scoped_lock single(m1);
    }
    {
        std::unique_lock again(m1);
        std::cout << again.owns_lock() << '\n';          // true —— 已释放
    }

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 双向并发转账：A->B 与 B->A 各 1000 笔，每笔 1 元
    //     余额给足（每笔必然成功）→ 无论怎样交错，双向净额都恰为 0，
    //     结果确定；顺序相反的裸加锁有死锁风险，这里统一使用 scoped_lock
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
    //      耗时只供观察，会受调度影响，不作为共享锁正确性的断言。
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
    std::cout << "reader elapsed (ms): " << ms << '\n';   // 具体数值随运行环境变化
    std::cout << cache.get("k") << '\n';                 // v

    return 0;
}
