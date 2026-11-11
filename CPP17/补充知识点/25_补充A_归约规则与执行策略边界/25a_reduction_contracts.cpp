// 补充 25A：归约的分组规则、无共享写入的聚合，以及 seq 的异常边界。
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <execution>
#include <functional>
#include <iostream>
#include <numeric>
#include <stdexcept>

struct Stats {
    std::size_t valid = 0;
    std::size_t rejected = 0;
    long long total = 0;
};

Stats inspect(int reading) noexcept {
    if (reading < 0 || reading > 100) return {0, 1, 0};
    return {1, 0, reading};
}

Stats combine(Stats left, Stats right) noexcept {
    return {left.valid + right.valid, left.rejected + right.rejected,
            left.total + right.total};
}

bool same(Stats left, Stats right) noexcept {
    return left.valid == right.valid && left.rejected == right.rejected
        && left.total == right.total;
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";
    const std::array<int, 3> numbers{1, 2, 3};
    const int ordered = std::accumulate(numbers.begin(), numbers.end(), 0, std::minus<>{});
    const int regrouped = (0 - 1) - (2 - 3);
    std::cout << "left fold: " << ordered << '\n';       // -6
    std::cout << "another grouping: " << regrouped << '\n'; // 0
    // 不断言 reduce(seq, ..., minus) 返回哪个值：它仍允许重组。
    require(ordered == -6 && regrouped == 0, "grouping example failed");

    std::cout << "===== 2. SYNTAX =====\n";
    const std::array<int, 6> readings{12, -1, 18, 25, -7, 10};
    const Stats sequential = std::transform_reduce(
        std::execution::seq, readings.begin(), readings.end(), Stats{}, combine, inspect);
    const Stats parallel = std::transform_reduce(
        std::execution::par, readings.begin(), readings.end(), Stats{}, combine, inspect);
    const Stats empty = std::transform_reduce(
        std::execution::seq, readings.begin(), readings.begin(), Stats{}, combine, inspect);
    require(same(sequential, Stats{4, 2, 65}) && same(sequential, parallel),
            "aggregation failed");
    require(same(empty, Stats{}), "identity failed");
    std::cout << "valid/rejected/total: " << parallel.valid << ' '
              << parallel.rejected << ' ' << parallel.total << '\n'; // 4 2 65
    std::cout << "seq/par agree: " << same(sequential, parallel) << '\n'; // true

    std::cout << "===== 3. APPLICATION =====\n";
    // 无执行策略的重载，元素函数异常可以正常传播。
    bool caught = false;
    try {
        std::for_each(readings.begin(), readings.end(), [](int) {
            throw std::runtime_error("demonstration");
        });
    } catch (const std::runtime_error&) {
        caught = true;
    }
    require(caught, "ordinary overload should propagate");
    std::cout << "ordinary exception caught: " << caught << '\n'; // true
#ifdef LESSON25A_THROW
    // 诊断实验在单独进程运行，用明确退出码避免崩溃弹窗或转储。
    std::set_terminate([] {
        std::cerr << "policy terminate\n";
        std::_Exit(86);
    });
    std::for_each(std::execution::seq, readings.begin(), readings.end(), [](int) {
        throw std::runtime_error("seq policy also terminates");
    });
    return 1; // 若错误地正常返回，测试不能通过。
#endif
    std::cout << "checks: PASS\n";
}
