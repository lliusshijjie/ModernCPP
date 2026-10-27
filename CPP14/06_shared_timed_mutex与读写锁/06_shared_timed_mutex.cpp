// 第 06 课示例：std::shared_timed_mutex 与读写锁（C++14）
//
// 结构：① 原理验证 —— try 系列实证四种持锁关系
//       ② 语法演示 —— adopt_lock / timed 接口 / release 交还控制权
//       ③ 应用实战 —— 读写缓存；双读者计时实证并行
//
// 编译运行：
//   g++ -std=c++14 -Wall -Wextra 06_shared_timed_mutex.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

// ③ 读多写少的教科书场景：共享配置缓存
class ConfigCache {
public:
    std::string get(const std::string& key) const {
        std::shared_lock<std::shared_timed_mutex> r(mtx_);   // 读：共享锁
        return map_.at(key);
    }
    void set(const std::string& key, std::string val) {
        std::unique_lock<std::shared_timed_mutex> w(mtx_);   // 写：独占锁
        map_[key] = std::move(val);
    }
private:
    mutable std::shared_timed_mutex mtx_;   // const 接口里也要加锁 → mutable
    std::map<std::string, std::string> map_;
};

int main() {
    std::cout << std::boolalpha;
    std::shared_timed_mutex mtx;

    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：独占写锁压制共享读锁
    {
        std::unique_lock<std::shared_timed_mutex> writer(mtx);
        std::cout << mtx.try_lock_shared() << '\n';    // false —— 写者持有，读者进不来
    }                                                  // writer 析构 → 解锁

    // 验证 2：释放后共享锁立刻可得
    {
        std::shared_lock<std::shared_timed_mutex> reader(mtx);
        std::cout << "reader holds\n";                 // reader holds
    }

    // 验证 3：读者与读者并存
    {
        std::shared_lock<std::shared_timed_mutex> r1(mtx);
        std::shared_lock<std::shared_timed_mutex> r2(mtx, std::defer_lock);
        std::cout << r2.try_lock() << '\n';            // true —— 读者可并存
    }

    // 验证 4：写者与写者互斥
    {
        std::unique_lock<std::shared_timed_mutex> w1(mtx);
        std::unique_lock<std::shared_timed_mutex> w2(mtx, std::defer_lock);
        std::cout << w2.try_lock() << '\n';            // false —— 写者互斥
    }

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 adopt_lock：把已持有的锁交给 RAII 接管（出作用域自动 unlock_shared）
    mtx.lock_shared();
    {
        std::shared_lock<std::shared_timed_mutex> r(mtx, std::adopt_lock);
    }
    std::cout << "adopt ok\n";                         // adopt ok

    // 2.2 timed 接口：锁空闲时 try_lock_shared_for 立即成功（不会真等满 50ms）
    bool got = mtx.try_lock_shared_for(std::chrono::milliseconds(50));
    std::cout << got << '\n';                          // true
    mtx.unlock_shared();

    // 2.3 release()：放弃 RAII 管辖，手动解锁（与 unique_ptr::release 同风格）
    std::shared_lock<std::shared_timed_mutex> r3(mtx);
    std::shared_timed_mutex* m = r3.release();         // r3 不再负责解锁
    std::cout << (r3.mutex() == nullptr) << '\n';      // true
    m->unlock_shared();                                // 手动还锁

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 读写缓存：4 个读者并行读，写者随后更新
    ConfigCache cache;
    cache.set("license", "alpha");
    std::vector<std::string> results(4);
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&, i] { results[i] = cache.get("license"); });
    }
    for (auto& t : readers) t.join();                  // 等全部读者完成
    for (const auto& s : results) std::cout << s << '\n';   // alpha × 4
    cache.set("license", "beta");                      // 写者独占更新
    std::cout << cache.get("license") << '\n';         // beta

    // 3.2 读者并行实证：两个读者各持共享锁睡 150ms
    //     并行 → 总耗时 ≈150ms；若是普通 mutex（串行）→ ≥300ms
    std::shared_timed_mutex rw;
    const auto hold = std::chrono::milliseconds(150);
    auto t0 = std::chrono::steady_clock::now();
    std::thread rA([&] {
        std::shared_lock<std::shared_timed_mutex> r(rw);
        std::this_thread::sleep_for(hold);
    });
    std::thread rB([&] {
        std::shared_lock<std::shared_timed_mutex> r(rw);
        std::this_thread::sleep_for(hold);
    });
    rA.join();
    rB.join();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - t0).count();
    std::cout << (ms < 250) << '\n';                   // true —— 读者真并行

    return 0;
}
