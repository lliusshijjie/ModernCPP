// 第 46 课示例：char8_t 与 Unicode 基础（C++20）
//
// 结构：① 原理验证 —— u8 字面量的新类型；char 的符号陷阱
//       ② 语法演示 —— u8string 与 char 的显式互转；码点计数
//       ③ 应用实战 —— UTF-8 前缀字节法解析
//
// 编译运行：
//   g++ -std=c++20 -Wall -Wextra 46_char8_unicode.cpp -o run && ./run
// 先预测每行输出，再运行对照！
// （输出全为 ASCII 数字/字母——中文原文以注释说明）

#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>

// u8 字面量：C++20 起元素类型是 char8_t（不再是 char）
static_assert(std::is_same_v<decltype(u8"a"[0]), const char8_t&>);
static_assert(sizeof(u8"你好") == 7);   // 你(3) + 好(3) + NUL(1)

// UTF-8 代码单元 -> ASCII 十六进制文本（不是编码转码）
std::string to_ascii_hex(const char8_t* s, std::size_t n) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (std::size_t i = 0; i < n; ++i) {
        unsigned b = static_cast<unsigned>(s[i]);   // char8_t 无符号
        out += digits[b >> 4];
        out += digits[b & 0xF];
    }
    return out;
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：本机默认 char 有符号；可由实现/编译选项改变，并非 x86 强制
    std::cout << static_cast<int>(static_cast<char>(0xE4)) << ' '   // -28
              << static_cast<int>(static_cast<char8_t>(0xE4)) << ' '  // 228
              << '\n';
    // char 可能读成负数；需要代码单元数值时先转 unsigned char
    static_assert(std::is_convertible_v<char8_t, char>); // 数值可隐式转换
    static_assert(!std::is_convertible_v<const char8_t*, const char*>); // 指针不可以

    // 验证 2：u8 字面量是 char8_t 序列；"你好" 每字 3 字节
    // 注意：auto 会把数组退化成指针（sizeof 变指针大小）——要保数组用显式声明
    const char8_t greet[] = u8"你好";
    std::cout << sizeof(greet) << '\n';            // 7（含 NUL）
    // auto ptr = u8"你好";
    // std::cout << sizeof(ptr);   // 4：指针（32 位目标）——数组已退化

    // 验证 3：指针和 string 类型之间无隐式通道（不等于整数值不能转换）
    // std::cout << greet;                  // 编译错：没有 char8_t 的 <<
    // std::u8string u{greet};
    // std::string s = u;                   // 编译错：u8string -> string 无隐式转换
    // const char* p = u8"x";              // 编译错（C++17 可以，C++20 不行）

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 u8string / u8string_view；filesystem::path 支持 u8string，ostream 无直接插入器
    std::u8string name{u8"cpp20"};
    std::cout << name.size() << '\n';              // 5（ASCII 区 1 字节/字符）

    // 2.2 字节级输出走显式转换（hex 化）
    std::cout << to_ascii_hex(greet, 6) << '\n';   // e4bda0e5a5bd
    // 你 = E4 BD A0；好 = E5 A5 BD（UTF-8 编码，十六进制）

    // 2.3 前提：输入已验证为合法 UTF-8。此计数器不是 UTF-8 验证器
    //     此前提下，非 10xxxxxx 开头的字节才对应新码点
    auto count_code_points = [](std::u8string_view s) {
        std::size_t n = 0;
        for (char8_t c : s)
            if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
        return n;
    };
    std::u8string_view mixed{u8"a你好b"};
    std::cout << mixed.size() << ' '                // 8（1+3+3+1 字节）
              << count_code_points(mixed) << '\n'; // 4（码点数）

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 UTF-8 长度表（由前缀字节判当前码点的字节数）
    constexpr auto seq_len = [](char8_t c) -> int {
        unsigned b = static_cast<unsigned>(c);
        if ((b & 0x80) == 0x00) return 1;          // 0xxxxxxx
        if (b >= 0xC2 && b <= 0xDF) return 2;     // 排除过长编码前缀 C0/C1
        if (b >= 0xE0 && b <= 0xEF) return 3;
        if (b >= 0xF0 && b <= 0xF4) return 4;     // 排除超过 Unicode 上限的前缀
        return 0;                                 // 非法首字节；仍需验证后续字节和值域
    };
    static_assert(seq_len(char8_t{0xC0}) == 0 && seq_len(char8_t{0xC1}) == 0);
    static_assert(seq_len(char8_t{0x80}) == 0 && seq_len(char8_t{0xF5}) == 0);
    static_assert(seq_len(char8_t{0xF4}) == 4);     // 长度合法仍不代表整个序列合法
    std::u8string_view text{u8"a好\U0001F600"};    // a + 好 + emoji(U+1F600)
    std::cout << seq_len(text[0]) << ' '           // 1
              << seq_len(text[1]) << ' '           // 3
              << seq_len(text[4]) << '\n';         // 4
    std::cout << text.size() << ' '                // 8 = 1+3+4 字节
              << count_code_points(text) << '\n';  // 3 码点

    // 3.2 家庭 emoji：三个图形码点加两个 ZWJ，共 5 个码点、18 个 UTF-8 字节
    std::u8string_view family{u8"\U0001F468\u200D\U0001F469\u200D\U0001F467"};
    std::cout << family.size() << ' ' << count_code_points(family) << '\n'; // 18 5

    // 3.3 实战心法（对照 ①②③）：
    //   字节处理用 unsigned/char8_t，别让 signed char 进位运算
    //   字节数 != 码点数 != 字素数（emoji 组合字素更多）——
    //   "长度"先问清量的是什么
    //   char8_t 是"这是 UTF-8"的类型化声明；复杂 Unicode 上库
    //   （ICU / Boost.Locale / uni-algo）
    return 0;
}
