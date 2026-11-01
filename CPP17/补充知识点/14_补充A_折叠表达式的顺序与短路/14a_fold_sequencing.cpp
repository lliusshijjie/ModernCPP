// 补充 14A：折叠表达式的顺序与短路（C++17）。
// ① 原理验证 -> ② 语法演示 -> ③ 应用实战；输出仅使用 ASCII。
// 从 ModernCPP 目录编译：
// g++ -std=c++17 -Wall -Wextra -Wpedantic "CPP17/补充知识点/14_补充A_折叠表达式的顺序与短路/14a_fold_sequencing.cpp" -o run

#include <iostream>
#include <string>
#include <type_traits>
#include <utility>

template <class... F>
bool run_all(F&&... checks) {
    // 转为 bool，保证使用内建 &&；短路发生在这些调用之间。
    return (... && static_cast<bool>(std::forward<F>(checks)()));
}
template <class... F>
void run_each(F&&... steps) {
    // 转为 void，保证使用内建逗号运算符，从左到右调用。
    (static_cast<void>(std::forward<F>(steps)()), ...);
}
template <class... B>
bool packed_all(B... values) { return (... && static_cast<bool>(values)); }
template <class Init, class... T>
auto sum_from(Init initial, T... values) { return (initial + ... + values); }

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";
    std::string log;
    auto step = [&log](char label, bool accepted) {
        return [&log, label, accepted] { log += label; return accepted; };
    };
    const bool accepted = run_all(step('A', true), step('B', false), step('C', true));
    std::cout << accepted << ' ' << log << '\n'; // false AB；C 没有被调用。
    if (accepted || log != "AB") return 1;
    log.clear();
    run_each(step('A', true), step('B', false), step('C', true));
    std::cout << log << '\n'; // ABC；逗号折叠会依次执行全部调用。
    if (log != "ABC") return 1;

    std::cout << "===== 2. SYNTAX =====\n";
    std::cout << run_all() << '\n'; // true；内建 && 的空包结果。
    run_each(); // 合法；空包逗号折叠不执行任何操作。
    static_assert(std::is_same_v<decltype(sum_from(0)), int>);
    static_assert(std::is_same_v<decltype(sum_from(0.0)), double>);
    std::cout << sum_from(0) << ' ' << sum_from(0.0, 1, 2) << '\n'; // 0 3
    std::cout << sum_from(std::string{}, "ab", "cd") << '\n'; // abcd

    std::cout << "===== 3. APPLICATION =====\n";
    int eager_calls = 0;
    auto eager = [&eager_calls](bool result) { ++eager_calls; return result; };
    const bool eager_result = packed_all(eager(false), eager(true));
    // 两个实参都在进入 packed_all 前执行；此处只核对次数，不假设其先后顺序。
    std::cout << eager_result << ' ' << eager_calls << '\n'; // false 2
    int lazy_calls = 0;
    const bool lazy_result = run_all(
        [&lazy_calls] { ++lazy_calls; return false; },
        [&lazy_calls] { ++lazy_calls; return true; });
    std::cout << lazy_result << ' ' << lazy_calls << '\n'; // false 1
    if (eager_result || eager_calls != 2 || lazy_result || lazy_calls != 1) return 1;
    // (... + make_value(xs)) 只有结合方式，不保证 make_value(xs) 从左到右执行。
    // 短路不会免除后续表达式的类型检查；每个被展开的调用仍须合法。
}
