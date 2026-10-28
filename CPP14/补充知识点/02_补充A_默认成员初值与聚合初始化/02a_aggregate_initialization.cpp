// 补充 02A：默认成员初值与聚合初始化（C++14）
// ① 原理验证 -> ② 语法演示 -> ③ 应用实战；输出仅使用 ASCII。
// 从 ModernCPP 目录编译：
// g++ -std=c++14 -Wall -Wextra -Wpedantic "CPP14/补充知识点/02_补充A_默认成员初值与聚合初始化/02a_aggregate_initialization.cpp" -o run

#include <iostream>

struct Config {
    int retries = 3;
    int timeout_ms = 100;
    bool trace = false;
};

struct Limits {
    int base = 10;
    int doubled = base * 2;
};

struct Plain {
    int count;
    bool enabled;
};

struct WorkerOptions {
    Config config{};
    int max_jobs = 8;
};

constexpr Config defaults{};
constexpr Config custom{5};
static_assert(defaults.retries == 3, "default member initializer");
static_assert(defaults.timeout_ms == 100, "default timeout");
static_assert(custom.retries == 5, "explicit value wins");
static_assert(custom.timeout_ms == 100, "omitted member keeps its default");

void print_config(const Config& config) {
    std::cout << config.retries << ' ' << config.timeout_ms << ' '
              << config.trace << '\n';
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";
    print_config(Config{});              // 3 100 false
    print_config(Config{5});             // 5 100 false
    print_config(Config{2, 250, true});   // 2 250 true

    std::cout << "===== 2. SYNTAX =====\n";
    const Limits derived{7};
    const Limits explicit_value{7, 99};
    std::cout << derived.base << ' ' << derived.doubled << '\n';   // 7 14
    std::cout << explicit_value.doubled << '\n';                   // 99

    const Plain zero{};
    std::cout << zero.count << ' ' << zero.enabled << '\n';        // 0 false
    // Plain uninitialized; 不要读取其中未初始化的标量成员。
    // Config narrowing{2.5}; // 窄化转换，编译错误。

    std::cout << "===== 3. APPLICATION =====\n";
    const WorkerOptions worker{{4, 200, true}, 6};
    print_config(worker.config);                                 // 4 200 true
    std::cout << worker.max_jobs << '\n';                         // 6

    Config named{};
    named.timeout_ms = 500;  // 多个相似字段时，按名称赋值便于阅读。
    print_config(named);                                         // 3 500 false
}
