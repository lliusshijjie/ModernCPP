#include "detail.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <execution>
#include <fstream>

namespace assetforge::detail {
Bytes read_file(const fs::path& file, Size limit) {
    if (!fs::is_regular_file(file)) {
        throw Error("expected regular file: " + file.u8string());
    }
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        throw Error("cannot open: " + file.u8string());
    }

    Bytes bytes;
    std::array<char, 32 * 1024> chunk;
    while (input) {
        input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        const auto count = static_cast<std::size_t>(input.gcount());
        if (count > limit - bytes.size()) {
            throw Error("byte limit exceeded: " + file.u8string());
        }
        if (count) {
            const auto offset = bytes.size();
            bytes.resize(offset + count);
            std::memcpy(bytes.data() + offset, chunk.data(), count);
        }
    }
    if (input.bad() || !input.eof()) {
        throw Error("read failed: " + file.u8string());
    }
    return bytes;
}

void read_exact(std::istream& stream, Bytes& bytes) {
    if (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()),
                                       static_cast<std::streamsize>(bytes.size()))) {
        throw Error("cannot read full bundle block");
    }
}

std::uint32_t crc32(Block block) noexcept {
    std::uint32_t value = 0xffffffffu;
    for (std::size_t i = 0; i < block.size; ++i) {
        auto byte = std::to_integer<unsigned>(block.data[i]);
        value = (value >> 8) ^ crc_table[(value ^ byte) & 0xffu];
    }
    return value ^ 0xffffffffu;
}

std::vector<std::uint32_t> checksums(const std::vector<Block>& blocks, Policy policy) {
    // [25] Buffers and result slots already exist when the policy algorithm starts.
    std::vector<std::uint32_t> result(blocks.size());
    if (policy == Policy::par) {
        std::transform(std::execution::par, blocks.begin(), blocks.end(), result.begin(), crc32);
    } else {
        std::transform(std::execution::seq, blocks.begin(), blocks.end(), result.begin(), crc32);
    }
    return result;
}
} // namespace assetforge::detail
