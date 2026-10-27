// 第 09 课示例：二进制字面量、数字分隔符、[[deprecated]]（C++14）
//
// 结构：① 原理验证 —— 纯记法：与十/十六进制值逐一相等
//       ② 语法演示 —— deprecated 定义不警告、使用才警告（体验行可自行取消注释）
//       ③ 应用实战 —— 权限位 + 报文字段的位操作
//
// 编译运行：
//   g++ -std=c++14 -Wall -Wextra 09_binary_literals.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <cstdint>
#include <iostream>

// ② deprecated：定义不触发警告 —— 本文件零警告的关键
[[deprecated("use new_calc(int) instead; old_calc will be removed in v3")]]
double old_calc(double x) { return x * 2.0; }

double new_calc(int x) { return x * 2.0; }

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：二进制字面量是纯记法 —— 与十六进制值完全相等
    static_assert(0b1010 == 10, "");
    static_assert(0b1010'1010 == 0xAA, "");
    std::cout << (0b1010'1010 == 0xAA) << '\n';    // true

    // 验证 2：分隔符是纯词法特性 —— 值不变，只服务人眼
    static_assert(1'000'000 == 1000000, "");
    static_assert(0xFF'FF == 65535, "");
    std::cout << 1'000'000'007 << '\n';            // 1000000007（著名大素数）

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 deprecated 只在"使用"时警告：定义+不调用 = 零警告
    std::cout << new_calc(21) << '\n';             // 42

    // 取消下一行注释，编译即得警告（体验 [[deprecated]] 的迁移引导）：
    // std::cout << old_calc(21.0) << '\n';
    // warning: 'double old_calc(double)' is deprecated: use new_calc(int)
    // instead; old_calc will be removed in v3 [-Wdeprecated-declarations]

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 权限位：二进制字面量让每个标志一眼可读
    constexpr unsigned perm_r = 0b100;             // 读
    constexpr unsigned perm_w = 0b010;             // 写
    constexpr unsigned perm_x = 0b001;             // 执行
    unsigned perm = perm_r | perm_w;               // 读+写 = 0b110
    std::cout << perm << '\n';                     // 6
    std::cout << ((perm & perm_x) != 0) << '\n';   // false —— 未授权执行位

    // 3.2 报文字段：分隔符按字节分组，拆位直接移位
    std::uint32_t packet = 0xAA'BB'CC'DD;
    std::cout << std::hex << packet << '\n';       // aabbccdd
    std::cout << std::dec << (packet >> 16) << '\n';   // 43707（高 16 位 0xAABB）

    return 0;
}
