// 补充 40A：用同一接口实现编译期与运行时的字节清零。

#include <array>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace {

constexpr bool evaluating_constant() noexcept {
    return std::is_constant_evaluated();
}

constexpr void clear_bytes(std::span<unsigned char> bytes) noexcept {
    if (bytes.empty()) {
        return;
    }

    if (std::is_constant_evaluated()) {
        for (auto& byte : bytes) {
            byte = 0;
        }
    } else {
        std::memset(bytes.data(), 0, bytes.size());
    }
}

constexpr auto cleared_middle() {
    std::array<unsigned char, 4> data{7, 8, 9, 10};
    clear_bytes(std::span{data}.subspan<1, 2>());
    return data;
}

void check(bool condition) {
    if (!condition) {
        throw std::runtime_error("byte clearing check failed");
    }
}

} // namespace

int main() {
    std::cout << std::boolalpha;

    // ① 原理验证：同一函数可出现在两种求值上下文中。
    constexpr auto at_compile_time = evaluating_constant();
    bool at_runtime = evaluating_constant();
    static_assert(at_compile_time);
    check(!at_runtime);
    std::cout << "constant context=" << at_compile_time << ' ' << at_runtime
              << '\n'; // constant context=true false

    // ② 语法演示：返回的数组可作为常量使用。
    constexpr auto expected = cleared_middle();
    static_assert(expected == std::array<unsigned char, 4>{7, 0, 0, 10});
    std::cout << "compile middle sum=" << (expected[1] + expected[2])
              << '\n'; // compile middle sum=0

    // ③ 应用实战：运行时清理同一子区间，检查两侧哨兵字节。
    std::array<unsigned char, 4> buffer{7, 3, 5, 10};
    clear_bytes(std::span{buffer}.subspan(1, 2));
    clear_bytes(std::span<unsigned char>{});
    check(buffer == expected);
    std::cout << "runtime matches=" << (buffer == expected) << '\n'; // runtime matches=true
}
