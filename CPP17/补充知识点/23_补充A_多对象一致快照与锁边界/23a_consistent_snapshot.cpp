// 补充 23A：同时更新两个计数器；读取者取得同一时刻的一致快照。
#include <future>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <utility>

struct Counter {
    mutable std::shared_mutex mutex;
    int value = 0;
};

void advance(Counter& first, Counter& second) {
    if (&first == &second) { // 别名参数只锁一次，避免递归获取同一把锁。
        std::unique_lock lock(first.mutex);
        ++first.value;
        return;
    }
    std::scoped_lock lock(first.mutex, second.mutex); // 两把都是独占锁。
    ++first.value;
    ++second.value;
}

std::pair<int, int> snapshot(const Counter& first, const Counter& second) {
    if (&first == &second) {
        std::shared_lock lock(first.mutex);
        return {first.value, first.value};
    }
    std::shared_lock left(first.mutex, std::defer_lock);
    std::shared_lock right(second.mutex, std::defer_lock);
    std::lock(left, right); // 对锁包装器操作，实际取得两把共享锁。
    return {first.value, second.value}; // 复制后再释放锁，不返回内部引用。
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        std::cout << "===== 1. PRINCIPLES =====\n";
        Counter single;
        advance(single, single);
        const auto [one, same] = snapshot(single, single);
        require(one == 1 && same == 1, "alias handling failed");
        std::cout << "alias snapshot: " << one << ' ' << same << '\n'; // 1 1

        std::cout << "===== 2. SYNTAX =====\n";
        Counter left;
        Counter right;
        const auto before = snapshot(left, right); // 独立的值，不随原对象改变。
        constexpr int rounds = 1000;
        // future 比 Counter 后声明，异常退出时也先等待异步任务结束。
        auto writer1 = std::async(std::launch::async, [&] {
            for (int i = 0; i < rounds; ++i) advance(left, right);
        });
        auto writer2 = std::async(std::launch::async, [&] {
            for (int i = 0; i < rounds; ++i) advance(right, left);
        });
        auto reader = std::async(std::launch::async, [&] {
            for (int i = 0; i < 1500; ++i) {
                const auto [a, b] = snapshot(left, right);
                require(a == b, "inconsistent pair");
            }
        });
        writer1.get();
        writer2.get();
        reader.get(); // 任务异常由 get() 传回主线程。
        std::cout << "reader checks: PASS\n";

        std::cout << "===== 3. APPLICATION =====\n";
        const auto [a, b] = snapshot(left, right);
        require(a == 2 * rounds && b == 2 * rounds, "lost update");
        require(before.first == 0 && before.second == 0, "snapshot was not a value");
        std::cout << "before: " << before.first << ' ' << before.second << '\n';
        std::cout << "after: " << a << ' ' << b << '\n'; // 2000 2000
        std::cout << "checks: PASS\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
