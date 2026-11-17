// 第 42 课示例：<bit> 家族：popcount、bit_cast、endian（C++20）
//
// 结构：① 原理验证 —— 位计数三兄弟；位取整；端序判定
//       ② 语法演示 —— bit_cast 类型双关；rotl；无符号约束
//       ③ 应用实战 —— 对齐桶；浮点位级剖析
//
// 编译运行：
//   g++ -std=c++20 -Wall -Wextra 42_bit_family.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <bit>
#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>

struct Sample {                        // ③ 位级剖析的对象
    float value;
};

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：位计数三兄弟 + popcount
    std::uint32_t x = 0b1011'0000'0000'0000'0000'0000'0000'0000u;
    std::cout << std::popcount(x) << ' '              // 3（1 的个数）
              << std::countl_zero(x) << ' '          // 0（bit31 是 1：无前导 0）
              << std::countr_zero(x) << ' '          // 28（尾部 0）
              << std::countl_one(x) << ' '           // 1（前导 1：最高位起）
              << std::countr_one(x) << '\n';         // 0（尾部 1）

    // 验证 2：位取整与宽度
    std::cout << std::bit_width(x) << ' '            // 32（最高位是 bit31：31+1）
              << std::has_single_bit(0x100u) << ' '  // true（2 的幂）
              << std::bit_floor(0b1101u) << ' '      // 8（13 内最大 2 幂）
              << std::bit_ceil(0b1101u) << '\n';     // 16（不小于 13 的最小 2 幂）

    // 验证 3：端序判定（本机 x86/x64：小端）
    constexpr bool little = std::endian::native == std::endian::little;
    std::cout << little << '\n';                     // true
    static_assert(std::endian::native == std::endian::little,
                  "本课示例假定小端环境");

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 bit_cast：同大小平凡类型间的位级重解释（constexpr 可用）
    float f = 1.0f;
    auto bits = std::bit_cast<std::uint32_t>(f);
    std::cout << std::hex << bits << std::dec << '\n';   // 3f800000（IEEE754 的 1.0f）
    auto back = std::bit_cast<float>(0x3f800000u);
    std::cout << back << '\n';                       // 1（无损往返）

    // 2.2 循环左移：rotl（对 32 位，移 33 位 = 移 1 位）
    std::uint8_t b = 0b1000'0001;
    std::cout << +static_cast<unsigned>(std::rotl(b, 1)) << ' '   // 3（0b11）
              << +static_cast<unsigned>(std::rotr(b, 1)) << '\n'; // 192
    // "+unsigned" 避免 uint8_t 被当字符打印（输出保持 ASCII 数字）

    // 2.3 只收无符号：有符号实参被约束拒绝（编译错注释保存）
    // int signed_v = 42;
    // std::popcount(signed_v);   // 编译错：popcount 约束为无符号整数类型
    // （要数有符号数的位，先 static_cast<unsigned>——位模式不变）

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 对齐：countl_zero 把数值映到"下一个 2 的幂桶"
    auto bucket = [](std::size_t n) {
        return n <= 1 ? 0u : std::bit_width(n - 1);   // 32 路分桶下标
    };
    std::cout << bucket(1) << bucket(2) << bucket(3)
              << bucket(4) << bucket(7) << bucket(8)
              << bucket(9) << '\n';                   // 0122334
    // 1→0, 2→1, 3~4→2, 7~8→3, 9→4（bit_width(n-1) = log2 上取整）

    // 3.2 浮点位级剖析：拆 IEEE754 单精度（1+8+23 位）
    Sample s{1.5f};
    std::uint32_t u = std::bit_cast<std::uint32_t>(s.value);
    std::uint32_t sign = u >> 31;
    std::uint32_t expo = (u >> 23) & 0xFF;
    std::uint32_t frac = u & 0x7FFFFF;
    std::cout << sign << ' ' << expo << ' ' << frac << '\n';   // 0 127 4194304
    // 1.5 = +1.1b * 2^0 → 指数 127（0+127），尾数 0.1b = 2^22

    // 3.3 实战心法（对照 ①②③）：
    //   位计数/取整全走 <bit>——手写循环与 __builtin_* 都退休
    //   类型双关一律 bit_cast——memcpy 与指针双关（UB）让位
    //   端序用 std::endian 编译期判定——预处理宏再见
    return 0;
}
