// 第 35 课示例：三路比较 <=> 与统一比较（C++20）
//
// 结构：① 原理验证 —— 一行顶六个；类别自动推导；重写规则
//       ② 语法演示 —— 三种类别；NaN 三态；定制点压平
//       ③ 应用实战 —— 只有 <=> 的类型直接进 map/sort
//
// 编译运行：
//   g++ -std=c++20 -Wall -Wextra 35_spaceship.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <algorithm>
#include <compare>
#include <iostream>
#include <limits>
#include <map>
#include <type_traits>
#include <vector>

struct Ver {                       // 版本号：两个 default 全套比较到手
    int major, minor, patch;
    auto operator<=>(const Ver&) const = default;   // auto 推导类别
    bool operator==(const Ver&) const = default;
};

struct Score {                     // 含 double → 整体退化为 partial_ordering
    int id;
    double v;
    auto operator<=>(const Score&) const = default;
    bool operator==(const Score&) const = default;
};

struct Strict {                    // 手写压平：double 也强序（值域承诺无 NaN）
    double v;
    std::strong_ordering operator<=>(const Strict& o) const {
        return std::strong_order(v, o.v);
    }
    bool operator==(const Strict&) const = default;
};

// struct Bad { double v;
//     std::strong_ordering operator<=>(const Bad&) const = default; };
// 编译错（调用处才暴露）：defaulted 版本被隐式删除——
// double 的 partial_ordering 无法转换成 strong_ordering。

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：一行 <=> 顶六个运算符
    static_assert(std::is_same_v<decltype(Ver{} <=> Ver{}),
                                 std::strong_ordering>);
    std::cout << (Ver{1, 2, 3} < Ver{1, 3, 0}) << ' '     // true（minor 定序）
              << (Ver{1, 2, 3} == Ver{1, 2, 3}) << ' '    // true
              << (Ver{1, 2, 3} != Ver{1, 2, 4}) << '\n';  // true（!= 借 == 取反）

    // 验证 2：auto 类别推导——一个 double 成员拖垮全家
    static_assert(std::is_same_v<decltype(Score{} <=> Score{}),
                                 std::partial_ordering>);
    std::cout << (Score{1, 1.5} < Score{1, 2.5}) << '\n'; // true

    // 验证 3：重写规则——< 由 (a <=> b) < 0 合成，std::less 自动受益
    std::cout << std::is_lt(1.0 <=> 2.0) << ' '           // true
              << std::is_gteq(2.0 <=> 1.0) << '\n';       // true

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 partial_ordering：NaN 不可比；+0.0 与 -0.0 等价
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::cout << ((nan <=> 1.0) == std::partial_ordering::unordered) << ' '
              << ((0.0 <=> -0.0) == std::partial_ordering::equivalent)
              << '\n';                                    // true true

    // 2.2 定制点压平：同类型才可入参；NaN 依 IEEE totalOrder 定序
    std::cout << (std::strong_order(2.0, 2.0)
                      == std::strong_ordering::equal) << ' '        // true
              << (std::strong_order(nan, 1.0)
                      == std::strong_ordering::greater) << ' '      // true
              << (std::strong_order(-nan, 1.0)
                      == std::strong_ordering::less) << '\n';       // true
    // std::strong_order(2.0, 2.0f);  // 编译错：定制点要求同类型

    // 2.3 手写压平后的 Strict：比较不再出现 unordered
    std::cout << (Strict{1.5} < Strict{2.5}) << ' '       // true
              << (Strict{nan} > Strict{1.0}) << '\n';     // true（+NaN 居大）

    // 2.4 类别放宽：strong → weak/partial 合法，反向编译错
    std::weak_ordering w = Ver{1, 0, 0} <=> Ver{2, 0, 0};
    std::cout << std::is_lt(w) << '\n';                   // true

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 只有 <=> 的类型：map 的 std::less 借重写规则直接可用
    std::map<Ver, std::string> releases;
    releases[Ver{2, 0, 0}] = "modern";
    releases[Ver{1, 9, 5}] = "legacy";
    std::cout << releases.begin()->second << '\n';        // legacy（1.9.5 在前）
    std::cout << releases.contains(Ver{2, 0, 0}) << '\n'; // true

    // 3.2 排序/最值：无需任何手写比较器
    std::vector<Ver> vs{{1, 0, 1}, {0, 9, 9}, {1, 0, 0}};
    std::ranges::sort(vs);
    for (const auto& v : vs)
        std::cout << v.major << '.' << v.minor << '.' << v.patch << ' ';
    std::cout << '\n';                       // 0.9.9 1.0.0 1.0.1

    // 3.3 实战心法（对照 ①②③）：
    //   值语义类：<=> 与 == 成对 default，两行结束
    //   浮点成员先想类别：强序手写 strong_order 压平
    //   unordered 不是 false——下游分支要显式处理
    return 0;
}
