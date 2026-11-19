#pragma once
#include "assetforge/bundle.hpp"
#include <array>
#include <iosfwd>
#include <string_view>

namespace assetforge::detail {
template <class... F>
struct Overloaded : F... {
    using F::operator()...;
};
template <class... F>
Overloaded(F...) -> Overloaded<F...>; // [15]

struct Block {
    const std::byte* data;
    std::size_t size;
};

constexpr auto make_crc_table() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < table.size(); ++i) {
        auto value = i;
        for (unsigned bit = 0; bit < 8; ++bit) {
            value = (value >> 1) ^ ((value & 1u) ? 0xedb88320u : 0);
        }
        table[i] = value;
    }
    return table;
}

inline constexpr auto crc_table = make_crc_table(); // [26]

Bytes read_file(const fs::path& file, Size limit);
void read_exact(std::istream& stream, Bytes& bytes);
std::uint32_t crc32(Block block) noexcept;
std::vector<std::uint32_t> checksums(const std::vector<Block>& blocks, Policy policy);
void write_bundle(const fs::path&, const std::vector<Entry>&, const Names&,
                  const std::vector<Block>&, Size);

std::vector<Index> topological_order(const Catalog&, const std::vector<bool>& included);
std::string at_line(std::size_t line, std::string_view reason);
} // namespace assetforge::detail
