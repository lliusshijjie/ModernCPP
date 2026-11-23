// 补充 38A：一次就绪、重复阶段和有上限的资源借用。

#include <array>
#include <barrier>
#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <latch>
#include <semaphore>
#include <stdexcept>
#include <thread>

namespace {

void check(bool condition) {
    if (!condition) {
        throw std::runtime_error("thread coordination check failed");
    }
}

class Slot {
public:
    explicit Slot(std::counting_semaphore<2>& permits) : permits_(permits) {
        permits_.acquire();
    }

    Slot(const Slot&) = delete;
    Slot& operator=(const Slot&) = delete;

    ~Slot() {
        permits_.release();
    }

private:
    std::counting_semaphore<2>& permits_;
};

int wait_until_ready() {
    int value{};
    std::latch ready{1};

    std::jthread worker([&] {
        value = 42;
        ready.count_down();
    });
    ready.wait();
    return value;
}

std::array<int, 2> sum_two_rounds() {
    std::array<int, 2> slots{};
    std::array<int, 2> totals{};
    std::size_t round{};

    std::barrier phase{2, [&]() noexcept {
        totals[round++] = slots[0] + slots[1];
    }};

    std::jthread worker([&] {
        for (int partial : {7, 11}) {
            slots[1] = partial;
            phase.arrive_and_wait();
        }
    });

    for (int partial : {3, 5}) {
        slots[0] = partial;
        phase.arrive_and_wait();
    }
    worker.join();

    check(round == totals.size());
    return totals;
}

bool third_slot_available() {
    std::counting_semaphore<2> permits{2};
    std::latch worker_acquired{1};
    std::latch release_worker{1};

    Slot main_slot{permits};
    std::jthread worker([&] {
        Slot worker_slot{permits};
        worker_acquired.count_down();
        release_worker.wait();
    });

    worker_acquired.wait();
    const bool got_third = permits.try_acquire();
    if (got_third) {
        permits.release();
    }
    release_worker.count_down();
    worker.join();
    return got_third;
}

} // namespace

int main() {
    // ① 原理验证：读取发生在工作线程提交初始化结果之后。
    const auto value = wait_until_ready();
    check(value == 42);
    std::cout << "ready=" << value << '\n'; // ready=42

    // ② 语法演示：同一个 barrier 完成两轮归并。
    const auto totals = sum_two_rounds();
    check(totals == std::array{10, 16});
    std::cout << "round totals=" << totals[0] << ' ' << totals[1] << '\n'; // round totals=10 16

    // ③ 应用实战：已有两个借用者时，第三次借用失败。
    const bool available = third_slot_available();
    check(!available);
    std::cout << std::boolalpha << "third slot=" << available << '\n'; // third slot=false
}
