// 第 14 课示例：折叠表达式 fold expressions（C++17）
//
// 结构：① 原理验证 —— 四种形式；方向对减法的影响；空包规则
//       ② 语法演示 —— 流式打印 / 分隔符输出 / 逻辑聚合
//       ③ 应用实战 —— all/any；逐元素调用；折叠求最小值
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 14_fold_expressions.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <iostream>
#include <string>

// ① 二元右折叠：带初值 0，空包安全
template <class... Ts>
auto sum(Ts... xs) {
    return (xs + ... + 0);
}

// ① 方向演示：减法不可交换，左右折叠结果不同
template <class... Ts>
auto rsub(Ts... xs) {
    return (xs - ...);          // 1-(2-(3-4))
}

template <class... Ts>
auto lsub(Ts... xs) {
    return (... - xs);          // ((1-2)-3)-4
}

// ② 流式打印：cout 作初值的二元左折叠（退休第 03 课的 dummy 数组技巧）
template <class... Ts>
void print_all(const Ts&... xs) {
    (std::cout << ... << xs) << '\n';
}

// ② 带分隔符：逗号运算符折叠，每个元素前打印当前分隔符再更新它
template <class... Ts>
void print_csv(const Ts&... xs) {
    const char* sep = "";
    (((std::cout << sep << xs), sep = ", "), ...);
    std::cout << '\n';
}

// ② 逻辑聚合：&& 折叠，空包返回 true（单位元）
template <class... Ts>
bool all_positive(Ts... xs) {
    return (... && (xs > 0));
}

// ③ all / any
template <class... Bs>
bool all_of(Bs... bs) {
    return (bs && ...);
}

template <class... Bs>
bool any_of(Bs... bs) {
    return (bs || ...);
}

// ③ 逗号折叠：对每个参数调用 f（空包 → void()）
template <class F, class... Ts>
void for_each_arg(F f, const Ts&... xs) {
    (f(xs), ...);
}

// ③ 折叠求最小值：赋值放进三元，包在逗号折叠里
template <class T, class... Ts>
T vmin(T first, Ts... rest) {
    ((first = rest < first ? rest : first), ...);
    return first;
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：二元折叠带初值 —— 空包也不报错
    std::cout << sum(1, 2, 3, 4) << '\n';      // 10
    std::cout << sum() << '\n';                // 0（初值兜底）

    // 验证 2：方向对减法的影响 —— 同一包，两种折叠
    std::cout << rsub(1, 2, 3, 4) << '\n';     // -2（1-(2-(3-4))）
    std::cout << lsub(1, 2, 3, 4) << '\n';     // -8（((1-2)-3)-4）

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 流式折叠打印异构参数
    print_all(1, ' ', "two", ' ', 3.5);        // 1 two 3.5

    // 2.2 分隔符版本
    print_csv("a", "b", "c");                  // a, b, c

    // 2.3 逻辑折叠：&& 的空包单位元是 true
    std::cout << all_positive(1, 2, -3) << '\n';   // false
    std::cout << all_positive() << '\n';           // true

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 all / any
    std::cout << all_of(true, true, false) << ' '
              << any_of(false, true) << '\n';  // false true

    // 3.2 逐元素调用：统计字符串总长
    int total_len = 0;
    auto counter = [&total_len](const std::string& s) {
        total_len += static_cast<int>(s.size());
    };
    for_each_arg(counter, std::string("alpha"), std::string("beta"));
    std::cout << total_len << '\n';            // 9（5 + 4）

    // 3.3 折叠求最小值
    std::cout << vmin(4, 2, 9, -1, 7) << '\n'; // -1

    return 0;
}
