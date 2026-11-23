// 补充 45A：整份替换过滤配置，已启动任务继续使用自己的快照。

#include <atomic>
#include <iostream>
#include <latch>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {

struct Config {
    int minimum;
    std::string prefix;
};

using Snapshot = std::shared_ptr<const Config>;
using ConfigSlot = std::atomic<Snapshot>;

Snapshot make_config(int minimum, std::string prefix) {
    if (minimum < 0 || minimum > 5 || prefix.empty()) {
        throw std::invalid_argument("invalid filter config");
    }

    return std::make_shared<const Config>(Config{minimum, std::move(prefix)});
}

bool matches(const Config& config, int level, std::string_view message) {
    return level >= config.minimum && message.starts_with(config.prefix);
}

void check(bool condition) {
    if (!condition) {
        throw std::runtime_error("snapshot check failed");
    }
}

void compare_tasks(ConfigSlot& current, const Snapshot& next) {
    std::latch loaded{1};
    std::latch published{1};
    bool first_task_matches{};

    std::jthread first_task([&] {
        auto view = current.load(std::memory_order_acquire);
        loaded.count_down();
        published.wait();
        first_task_matches = matches(*view, 2, "net: ready");
    });

    loaded.wait();
    current.store(next, std::memory_order_release);
    published.count_down();
    first_task.join();

    auto new_view = current.load(std::memory_order_acquire);
    bool second_task_matches = matches(*new_view, 2, "net: ready");
    check(first_task_matches && !second_task_matches);
    std::cout << "old task=" << first_task_matches << " new task=" << second_task_matches
              << '\n'; // old task=true new task=false
}

} // namespace

int main() {
    std::cout << std::boolalpha;

    // ① 原理验证：交换槽位后，已复制的指针继续持有旧对象。
    ConfigSlot current{make_config(1, "net:")};
    auto previous = current.exchange(make_config(2, "net:"));
    auto active = current.load();
    check(previous->minimum == 1 && active->minimum == 2);
    std::cout << "old/current minimum=" << previous->minimum << ' ' << active->minimum
              << '\n'; // old/current minimum=1 2

    // ② 语法演示：候选配置先验证，失败时保留已发布的对象。
    bool rejected{};
    try {
        auto candidate = make_config(-1, "net:");
        current.store(std::move(candidate), std::memory_order_release);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && current.load(std::memory_order_acquire) == active);
    std::cout << "invalid update rejected=" << rejected << '\n'; // invalid update rejected=true

    // ③ 应用实战：两个任务对同一条消息使用不同版本的过滤规则。
    active.reset();
    auto next = make_config(3, "net:");
    compare_tasks(current, next);
}
