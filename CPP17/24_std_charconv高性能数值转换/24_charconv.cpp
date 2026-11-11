// 第 24 课示例：std::charconv 高性能数值转换（C++17）
//
// 结构：① 原理验证 —— to_chars 最短往返；from_chars 指针即进度
//       ② 语法演示 —— 部分解析 / 非法输入 / 进制；0x 陷阱现场
//       ③ 应用实战 —— 就地解析 CSV；定点精度输出
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 24_charconv.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <charconv>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：to_chars 整数 —— [buf, to) 是成功写入的区间，转换本身不分配。
    char buf[32];
    auto [to, ec] = std::to_chars(buf, buf + sizeof buf, 42);
    std::cout << std::string(buf, to) << ' ' << (to - buf) << ' '
              << (ec == std::errc{}) << '\n';                // 42 2 true

    // 验证 2：to_chars 浮点 —— 最短往返表示（不是 printf 的 6 位）
    auto [to2, ec2] = std::to_chars(buf, buf + sizeof buf, 3.14);
    std::cout << std::string(buf, to2) << '\n';              // 3.14

    // 验证 3：from_chars —— ptr 即消费进度，round-trip 完整
    std::string_view src = "2.5";
    double d = 0;
    auto [from, ec3] = std::from_chars(src.data(), src.data() + src.size(), d);
    std::cout << d << ' ' << (from == src.data() + src.size()) << '\n';  // 2.5 true

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 部分解析是一等公民："123abc" → 123，ptr 停在 'a'
    std::string_view s1 = "123abc";
    int n = 0;
    auto [f1, e1] = std::from_chars(s1.data(), s1.data() + s1.size(), n);
    std::cout << n << ' ' << (f1 == s1.data() + s1.size()) << ' '
              << *f1 << '\n';                                // 123 false a

    // 2.2 非法输入：值不被修改（n 保持 99），ec = invalid_argument
    std::string_view s2 = "abc";
    int m = 99;
    auto [f2, e2] = std::from_chars(s2.data(), s2.data() + s2.size(), m);
    std::cout << (e2 == std::errc::invalid_argument) << ' ' << m << '\n';  // true 99

    // 2.3 整数进制："ff" 按 16 进制 → 255（自己管进制，没有 0x）
    std::string_view s3 = "ff";
    int h = 0;
    auto [f3, e3] = std::from_chars(s3.data(), s3.data() + s3.size(), h, 16);
    std::cout << h << ' ' << (e3 == std::errc{}) << ' '
              << (f3 == s3.data() + s3.size()) << '\n';      // 255 true true

    // 2.4 陷阱现场："0xff" 按 16 进制 → 只读到 0，停在 'x'（不是 255！）
    std::string_view s4 = "0xff";
    int h2 = 0;
    auto [f4, e4] = std::from_chars(s4.data(), s4.data() + s4.size(), h2, 16);
    std::cout << h2 << ' ' << *f4 << '\n';                   // 0 x

    // 2.5 to_chars 指定进制：5 的二进制
    auto [to5, e5] = std::to_chars(buf, buf + sizeof buf, 5, 2);
    std::cout << std::string(buf, to5) << '\n';              // 101

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 就地解析 CSV："12,34,56" —— string_view 定位 + from_chars 转换
    //     直接解析字符范围；旧写法 substr + stoi 需要复制字符，可能堆分配。
    std::string_view csv = "12,34,56";
    int a = 0, b = 0, c = 0;
    auto seg = [&csv](int& out, std::size_t begin) {
        auto [f, e] = std::from_chars(csv.data() + begin, csv.data() + csv.size(), out);
        return f;                                            // 返回停点给下一段
    };
    auto p1 = seg(a, 0);
    auto p2 = seg(b, (p1 - csv.data()) + 1);                 // 跳过 ','
    auto p3 = seg(c, (p2 - csv.data()) + 1);
    bool all = (p1 - csv.data() == 2) && (p2 - csv.data() == 5) &&
               (p3 == csv.data() + csv.size());
    std::cout << a << ' ' << b << ' ' << c << ' ' << all << '\n';  // 12 34 56 true

    // 3.2 定点精度输出：替代 sprintf("%.2f")，无 locale、无分配
    auto [to6, e6] = std::to_chars(buf, buf + sizeof buf, 3.14159,
                                   std::chars_format::fixed, 2);
    std::cout << std::string(buf, to6) << '\n';              // 3.14

    return 0;
}
