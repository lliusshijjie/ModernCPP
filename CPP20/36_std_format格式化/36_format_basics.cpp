// 第 36 课示例：std::format 格式化（C++20）
//
// 结构：① 原理验证 —— 类型安全；编译期校验；bool 语义
//       ② 语法演示 —— 对齐/填充/符号/备选形式/嵌套域/转义
//       ③ 应用实战 —— 报表对齐；format_to/有界输出
//
// 编译运行：
//   g++ -std=c++20 -Wall -Wextra 36_format_basics.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <format>
#include <iostream>
#include <iterator>  // std::back_inserter
#include <string>
#include <vector>

int main() {
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：类型安全 + 返回 string；自动编号
    std::string s = std::format("{} x {}", "vec", 3);
    std::cout << s << ' ' << s.size() << '\n';     // vec x 3 7

    // 验证 2：格式串编译期校验——说明符与类型不符是编译错
    std::cout << std::format("{}", true) << ' '    // true（bool 输出单词）
              << std::format("{:d}", true) << '\n';// 1（按整数对待）
    // std::format("{:d}", "abc");   // 编译错：d 不接受字符串（P2216）
    // printf("%d", "abc");          // 旧世界：编译通过，运行 UB

    // 验证 3：位置编号与复用；自动/手动不可混用
    std::cout << std::format("{0}+{0}={1}", 2, 4) << '\n';   // 2+2=4
    // std::format("{} {0}", 1, 2);  // 编译错：混用自动与手动编号

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 对齐与填充：fill 紧贴 align；数值默认右对齐
    std::cout << '[' << std::format("{:>8}", "ab") << ']'
              << '[' << std::format("{:<8}", "ab") << ']'
              << '[' << std::format("{:^8}", "ab") << ']'
              << '[' << std::format("{:*^8}", "ab") << "]\n";
    // [      ab][ab      ][   ab   ][***ab***]

    // 2.2 数值：零填充（符号占宽）、显式正号、备选形式、科学计数
    std::cout << std::format("{:05}", -42) << ' '          // -0042
              << std::format("{:+.2f}", 3.14159) << ' '    // +3.14
              << std::format("{:#x} {:#b} {:#o}", 255, 5, 8) << ' '
              << std::format("{:e}", 12345.678) << '\n';   // 0xff 0b101 010 1.234568e+04

    // 2.3 字符串精度=截断；先截断后补宽
    std::cout << '[' << std::format("{:.3}", "abcdef") << ']'
              << '[' << std::format("{:10.3}", "abcdef") << "]\n";
    // [abc][abc       ]

    // 2.4 嵌套替换域：宽度/精度从实参来
    std::cout << std::format("{:*^{}}", "ab", 6) << ' '    // **ab**
              << std::format("{:.{}f}", 3.14159, 1) << '\n';  // 3.1

    // 2.5 花括号转义
    std::cout << std::format("{{:{}}}", "json") << '\n';   // {:json}

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 报表对齐：左对齐名称、右对齐金额——一次调用，零状态外溢
    struct Item { std::string name; double price; };
    std::vector<Item> menu{{"latte", 4.5}, {"americano", 3.25},
                           {"cappuccino", 4.875}};
    for (const auto& it : menu)
        std::cout << std::format("{:<12}{:>8.2f}\n", it.name, it.price);
    // latte           4.50
    // americano       3.25
    // cappuccino      4.88

    // 3.2 format_to：直写迭代器（免中间 string）
    std::string out;
    for (int i = 1; i <= 3; ++i)
        std::format_to(std::back_inserter(out), "[{:0>2}]", i);
    std::cout << out << '\n';                       // [01][02][03]

    // 3.3 有界输出：format_to_n 截断安全；size 是"全长"非写入长
    char buf[5];
    auto r = std::format_to_n(buf, 4, "{}", "abcdefg");
    buf[4] = '\0';                                  // 截断后自行收尾
    std::cout << buf << ' ' << r.size << ' '
              << std::formatted_size("{}", "abcdefg") << '\n';
    // abcd 7 7

    // 3.4 实战心法（对照 ①②③）：
    //   新代码一律 format；动态格式串走 vformat
    //   魔法宽度嵌套域参数化；bool 直接 {} 输出单词
    //   有界缓冲用 format_to_n；容器格式化等 C++23
    return 0;
}
