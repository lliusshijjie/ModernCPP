#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bloom17::detail {

struct Hash128 {
    std::uint64_t first;
    std::uint64_t second;
};

inline std::uint64_t rotate_left(std::uint64_t value, unsigned shift) noexcept {
    return (value << shift) | (value >> (64U - shift));
}

inline std::uint64_t avalanche(std::uint64_t value) noexcept {
    value ^= value >> 33U;
    value *= UINT64_C(0xff51afd7ed558ccd);
    value ^= value >> 33U;
    value *= UINT64_C(0xc4ceb9fe1a85ec53);
    return value ^ (value >> 33U);
}

inline std::uint64_t read_le64(const char* bytes) noexcept {
    std::uint64_t result = 0;

    for (unsigned i = 0; i < 8; ++i) {
        result |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[i])) << (8U * i);
    }

    return result;
}

// Portable MurmurHash3_x64_128, adapted from Austin Appleby's public-domain algorithm.
// https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
// Explicit byte loads avoid alignment, aliasing and host-endian dependencies.
inline Hash128 hash_bytes(std::string_view bytes, std::uint32_t seed) noexcept {
    constexpr auto c1 = UINT64_C(0x87c37b91114253d5);
    constexpr auto c2 = UINT64_C(0x4cf5ad432745937f);
    std::uint64_t first = seed;
    std::uint64_t second = seed;
    std::size_t offset = 0;

    while (bytes.size() - offset >= 16) {
        auto low = read_le64(bytes.data() + offset);
        auto high = read_le64(bytes.data() + offset + 8);

        first ^= rotate_left(low * c1, 31) * c2;
        first = (rotate_left(first, 27) + second) * 5 + UINT64_C(0x52dce729);
        second ^= rotate_left(high * c2, 33) * c1;
        second = (rotate_left(second, 31) + first) * 5 + UINT64_C(0x38495ab5);

        offset += 16;
    }

    std::uint64_t low = 0;
    std::uint64_t high = 0;
    const auto remaining = bytes.size() - offset;

    for (std::size_t i = 0; i < remaining; ++i) {
        const auto byte = static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[offset + i]));

        if (i < 8) {
            low |= byte << (8U * static_cast<unsigned>(i));
        } else {
            high |= byte << (8U * static_cast<unsigned>(i - 8));
        }
    }

    if (remaining > 8) {
        second ^= rotate_left(high * c2, 33) * c1;
    }

    if (remaining != 0) {
        first ^= rotate_left(low * c1, 31) * c2;
    }

    first ^= static_cast<std::uint64_t>(bytes.size());
    second ^= static_cast<std::uint64_t>(bytes.size());
    first += second;
    second += first;
    first = avalanche(first);
    second = avalanche(second);
    first += second;
    second += first;

    return {first, second};
}

inline Hash128 hash_u64(std::uint64_t value, std::uint32_t seed) noexcept {
    std::array<char, 8> bytes{};

    for (unsigned i = 0; i < 8; ++i) {
        bytes[i] = static_cast<char>((value >> (8U * i)) & 0xffU);
    }

    return hash_bytes(std::string_view(bytes.data(), bytes.size()), seed);
}

} // namespace bloom17::detail
