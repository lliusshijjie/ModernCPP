// 补充 37A：比较混合符号整数、检查窄化、验证二进制块长度。

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <variant>

namespace {

void check(bool condition) {
    if (!condition) {
        throw std::runtime_error("integer boundary check failed");
    }
}

std::optional<unsigned> byte_at(std::span<const std::byte> bytes, std::ptrdiff_t index) {
    if (index < 0 || !std::cmp_less(index, bytes.size())) {
        return std::nullopt;
    }

    return std::to_integer<unsigned>(bytes[static_cast<std::size_t>(index)]);
}

enum class LengthError { not_representable, over_limit, truncated };
using Length = std::variant<std::uint32_t, LengthError>;

Length checked_length(std::int64_t declared, std::size_t remaining) {
    constexpr std::uint32_t block_limit = 4096;
    if (!std::in_range<std::uint32_t>(declared)) {
        return LengthError::not_representable;
    }

    const auto length = static_cast<std::uint32_t>(declared);
    if (length > block_limit) {
        return LengthError::over_limit;
    }
    if (std::cmp_greater(length, remaining)) {
        return LengthError::truncated;
    }

    return length;
}

} // namespace

int main() {
    std::cout << std::boolalpha;

    // ① 原理验证：int 与 unsigned int 比较时，负数会先转成 unsigned int。
    constexpr int negative = -1;
    constexpr unsigned bound = 4;
    static_assert(!(static_cast<unsigned>(negative) < bound));
    static_assert(std::cmp_less(negative, bound));
    std::cout << "converted=" << (static_cast<unsigned>(negative) < bound)
              << " cmp_less=" << std::cmp_less(negative, bound) << '\n'; // false true

    // ② 语法演示：先验证下标，再转换；ssize 得到有符号长度。
    const std::array bytes{std::byte{12}, std::byte{34}, std::byte{56}};
    check(!byte_at(bytes, -1) && !byte_at(bytes, std::ssize(bytes)));
    check(byte_at(bytes, 0) == 12);
    std::cout << "reverse:";
    for (auto i = std::ssize(bytes); i > 0; --i) {
        std::cout << ' ' << byte_at(bytes, i - 1).value();
    }
    std::cout << '\n'; // reverse: 56 34 12

    // ③ 应用实战：类型边界、协议上限、剩余数据分别检查。
    constexpr auto beyond_u32 =
        static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()) + 1;
    check(checked_length(-1, bytes.size()) == Length{LengthError::not_representable});
    check(checked_length(beyond_u32, bytes.size()) == Length{LengthError::not_representable});
    check(checked_length(4097, 8192) == Length{LengthError::over_limit});
    check(checked_length(4, bytes.size()) == Length{LengthError::truncated});
    check(checked_length(0, 0) == Length{std::uint32_t{0}});
    check(checked_length(4096, 4096) == Length{std::uint32_t{4096}});
    std::cout << "accepted=" << std::get<std::uint32_t>(checked_length(3, bytes.size()))
              << " limit=" << std::get<std::uint32_t>(checked_length(4096, 4096))
              << '\n'; // accepted=3 limit=4096
}
