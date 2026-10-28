// 补充 05B：非成员迭代器与双范围算法（C++14）
// ① 原理验证 -> ② 语法演示 -> ③ 应用实战；输出仅使用 ASCII。
// 从 ModernCPP 目录编译：
// g++ -std=c++14 -Wall -Wextra -Wpedantic "CPP14/补充知识点/05_补充B_非成员迭代器与双范围算法/05b_iterator_range_tools.cpp" -o run

#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <type_traits>
#include <vector>

template <class Range>
int sum_readonly(const Range& range) {
    return std::accumulate(std::cbegin(range), std::cend(range), 0);
}

template <class Left, class Right>
bool same_records(const Left& left, const Right& right) {
    return std::equal(std::cbegin(left), std::cend(left),
                      std::cbegin(right), std::cend(right));
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";
    const std::vector<int> values{1, 2, 3};
    int array[]{4, 5, 6};
    static_assert(std::is_same<decltype(std::cbegin(array)), const int*>::value,
                  "non-member cbegin works with arrays");
    std::cout << sum_readonly(values) << ' ' << sum_readonly(array) << '\n'; // 6 15
    for (auto it = std::crbegin(array); it != std::crend(array); ++it) {
        std::cout << *it;
    }
    std::cout << '\n'; // 654

    std::cout << "===== 2. SYNTAX =====\n";
    const std::vector<int> longer{1, 2, 3, 99};
    std::cout << std::equal(values.begin(), values.end(), longer.begin())
              << '\n';                                // true：合法的前缀比较
    std::cout << same_records(values, longer) << '\n'; // false：完整范围长度不同
    // 不要交换三迭代器版的两个范围；较短的第二范围不满足接口前提。

    const std::vector<int> changed{1, 9, 3};
    const auto difference = std::mismatch(values.begin(), values.end(),
                                          changed.begin(), changed.end());
    if (difference.first != values.end() && difference.second != changed.end()) {
        std::cout << *difference.first << ' ' << *difference.second << '\n'; // 2 9
    }

    const std::vector<int> reordered{3, 1, 2};
    std::cout << std::is_permutation(values.begin(), values.end(),
                                     reordered.begin(), reordered.end())
              << '\n'; // true
    std::cout << std::is_permutation(values.begin(), values.end(),
                                     longer.begin(), longer.end())
              << '\n'; // false

    std::cout << "===== 3. APPLICATION =====\n";
    const int expected[]{1, 2, 3};
    std::cout << same_records(values, expected) << '\n'; // true：容器与数组统一比较
    const auto prefix = std::mismatch(values.begin(), values.end(),
                                      longer.begin(), longer.end());
    std::cout << (prefix.first == values.end()) << ' '
              << (prefix.second == longer.end()) << '\n'; // true false
}
