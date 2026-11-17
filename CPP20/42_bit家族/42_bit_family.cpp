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
#include <cstddef>
#include <cstdint>
#include <limits>
#include <iostream>
#include <string>
#include <type_traits>

struct Sample {                        // ③ 位级剖析的对象
    float value;
};

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 本例拆解 IEEE 754 binary32，并使用精确的 32 位无符号类型
    static_assert(sizeof(float) == sizeof(std::uint32_t));
    static_assert(std::numeric_limits<float>::is_iec559 &&
                  std::numeric_limits<float>::digits == 24 &&
                  std::numeric_limits<float>::max_exponent == 128);

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
    auto back = std::bit_cast<float>(std::uint32_t{0x3f800000u});
    std::cout << back << '\n';                       // 1（无损往返）

    // 2.2 循环左移：rotl（对 32 位，移 33 位 = 移 1 位）
    std::uint8_t b = 0b1000'0001;
    std::cout << +static_cast<unsigned>(std::rotl(b, 1)) << ' '   // 3（0b11）
              << +static_cast<unsigned>(std::rotr(b, 1)) << '\n'; // 192
    // "+unsigned" 避免 uint8_t 被当字符打印（输出保持 ASCII 数字）

    // 2.3 只收无符号：有符号实参被约束拒绝（编译错注释保存）
    // int signed_v = 42;
    // std::popcount(signed_v);   // 编译错：popcount 约束为无符号整数类型
    // 数有符号类型 T 的位，可先转对应的 make_unsigned_t<T>，避免改变宽度
    static_assert(std::bit_floor(0u) == 0u);
    static_assert(std::bit_ceil(0u) == 1u && std::bit_ceil(1u) == 1u);
    static_assert(std::bit_ceil(std::uint32_t{0x80000000u}) == 0x80000000u);
    // bit_ceil(uint32_t{0x80000001u}) 结果不可表示：运行期 UB，常量求值也不合法

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 分桶：bit_width(n-1) 给出 ceil(log2(n))；本例把 n<=1 都放入桶 0
    auto bucket = [](std::size_t n) {
        return n <= 1 ? 0 : std::bit_width(n - 1);   // 下标范围取决于 size_t 宽度
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
    //   同大小平凡类型的按位值转换用 bit_cast；memcpy 仍是合法的表示复制手段
    //   端序用 std::endian 编译期判定——预处理宏再见
    return 0;
}
