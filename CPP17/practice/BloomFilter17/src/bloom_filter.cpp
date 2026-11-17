#include "bloom17/bloom_filter.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <utility>

namespace bloom17 {
namespace {

constexpr std::size_t header_size = 40;
constexpr std::array<char, 8> magic{'B', 'L', 'M', '1', '7', '\r', '\n', '\x1a'};
constexpr std::uint32_t format_version = 1;
constexpr std::uint32_t hash_algorithm = 1; // MurmurHash3_x64_128 + odd double-hash step
constexpr std::uint32_t max_probes = 32;
constexpr long double ln2 = 0.693147180559945309417232121458176568L;

constexpr std::array<std::uint32_t, 256> crc_table() {
    std::array<std::uint32_t, 256> table{};

    for (std::size_t i = 0; i < table.size(); ++i) {
        auto value = static_cast<std::uint32_t>(i);

        for (unsigned bit = 0; bit < 8; ++bit) {
            value = (value >> 1U) ^ ((value & 1U) != 0 ? 0xedb88320U : 0U);
        }

        table[i] = value;
    }

    return table;
}

class Crc32 {
public:
    void update(const char* bytes, std::size_t count) noexcept {
        for (std::size_t i = 0; i < count; ++i) {
            const auto byte = static_cast<unsigned char>(bytes[i]);
            value_ = table_[(value_ ^ byte) & 0xffU] ^ (value_ >> 8U);
        }
    }

    std::uint32_t value() const noexcept {
        return ~value_;
    }

private:
    inline static constexpr auto table_ = crc_table();
    std::uint32_t value_ = 0xffffffffU;
};

void write_le(char* target, std::uint64_t value, unsigned width) noexcept {
    for (unsigned i = 0; i < width; ++i) {
        target[i] = static_cast<char>((value >> (i * 8U)) & 0xffU);
    }
}

std::uint64_t read_le(const char* source, unsigned width) noexcept {
    std::uint64_t value = 0;

    for (unsigned i = 0; i < width; ++i) {
        value |= static_cast<std::uint64_t>(static_cast<unsigned char>(source[i])) << (i * 8U);
    }

    return value;
}

void read_exact(std::istream& input, char* bytes, std::size_t count) {
    input.read(bytes, static_cast<std::streamsize>(count));

    if (!input) {
        throw std::runtime_error("Bloom snapshot: truncated or unreadable stream");
    }
}

void write_exact(std::ostream& output, const char* bytes, std::size_t count) {
    output.write(bytes, static_cast<std::streamsize>(count));

    if (!output) {
        throw std::runtime_error("Bloom snapshot: output write failed");
    }
}

// SWAR popcount: C++17 可移植实现，优化器可在目标支持时选择硬件指令。
std::size_t popcount(std::uint64_t word) noexcept {
    word -= (word >> 1U) & UINT64_C(0x5555555555555555);
    word = (word & UINT64_C(0x3333333333333333)) + ((word >> 2U) & UINT64_C(0x3333333333333333));
    word = (word + (word >> 4U)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
    return static_cast<std::size_t>((word * UINT64_C(0x0101010101010101)) >> 56U);
}

long double log_false_positive(std::size_t bits, std::uint32_t probes, std::size_t items) {
    const auto exponent = -static_cast<long double>(probes) * static_cast<long double>(items) /
                          static_cast<long double>(bits);
    return static_cast<long double>(probes) * std::log(-std::expm1(exponent));
}

} // namespace

BloomFilter::BloomFilter(const Options& options)
    : shape_(plan(options)), words_(shape_.bit_count / 64, 0) {
}

BloomFilter::BloomFilter(std::size_t expected_items, double false_positive_rate, std::uint32_t seed)
    : BloomFilter(Options{expected_items, false_positive_rate, seed, default_max_bytes}) {
}

BloomFilter::BloomFilter(Shape shape, std::vector<std::uint64_t> words) noexcept
    : shape_(shape), words_(std::move(words)) {
}

BloomFilter& BloomFilter::operator=(const BloomFilter& other) {
    if (this != &other) {
        auto copy = other;
        swap(copy);
    }

    return *this;
}

BloomFilter::BloomFilter(BloomFilter&& other) noexcept
    : shape_(std::exchange(other.shape_, Shape{})), words_(std::move(other.words_)) {
    other.words_.clear();
}

BloomFilter& BloomFilter::operator=(BloomFilter&& other) noexcept {
    if (this != &other) {
        shape_ = std::exchange(other.shape_, Shape{});
        words_ = std::move(other.words_);
        other.words_.clear();
    }

    return *this;
}

Shape BloomFilter::plan(const Options& options) {
    if (options.expected_items == 0 || !std::isfinite(options.false_positive_rate) ||
        options.false_positive_rate <= 0 || options.false_positive_rate >= 1) {
        throw std::invalid_argument(
            "Bloom options: expected_items > 0 and 0 < finite rate < 1 required");
    }

    const auto target_log = std::log(static_cast<long double>(options.false_positive_rate));
    const auto required =
        -static_cast<long double>(options.expected_items) * target_log / (ln2 * ln2);
    std::size_t bits = 64;

    const auto grow = [&] {
        if (bits > std::numeric_limits<std::size_t>::max() / 2 ||
            bits / 8 > options.max_bytes / 2) {
            throw std::length_error("Bloom options: requested shape exceeds memory budget");
        }

        bits *= 2;
    };

    while (static_cast<long double>(bits) < required) {
        grow();
    }

    for (;;) {
        if (bits / 8 > options.max_bytes) {
            throw std::length_error("Bloom options: requested shape exceeds memory budget");
        }

        // 向 2 的幂取整会多出空间，用这些空间减少随机内存探测次数。
        for (std::uint32_t probes = 1; probes <= max_probes; ++probes) {
            if (log_false_positive(bits, probes, options.expected_items) <= target_log) {
                return {bits, probes, options.seed};
            }
        }

        grow();
    }
}

void BloomFilter::require_active() const {
    if (shape_.bit_count == 0) {
        throw std::logic_error(
            "Bloom filter: moved-from object must be reassigned before mutation/save");
    }
}

void BloomFilter::clear() noexcept {
    std::fill(words_.begin(), words_.end(), UINT64_C(0));
}

void BloomFilter::swap(BloomFilter& other) noexcept {
    std::swap(shape_, other.shape_);
    words_.swap(other.words_);
}

bool BloomFilter::compatible_with(const BloomFilter& other) const noexcept {
    return shape_.bit_count != 0 && shape_.bit_count == other.shape_.bit_count &&
           shape_.probe_count == other.shape_.probe_count && shape_.seed == other.shape_.seed;
}

void BloomFilter::require_compatible(const BloomFilter& other) const {
    if (!compatible_with(other)) {
        throw std::invalid_argument("Bloom filters: bit count, probe count and seed must match");
    }
}

void BloomFilter::merge_from(const BloomFilter& other) {
    require_compatible(other);

    for (std::size_t i = 0; i < words_.size(); ++i) {
        words_[i] |= other.words_[i];
    }
}

void BloomFilter::intersect_with(const BloomFilter& other) {
    require_compatible(other);

    for (std::size_t i = 0; i < words_.size(); ++i) {
        words_[i] &= other.words_[i];
    }
}

Statistics BloomFilter::statistics() const noexcept {
    Statistics result;
    result.bit_count = shape_.bit_count;
    result.memory_bytes = words_.size() * sizeof(std::uint64_t);
    result.probe_count = shape_.probe_count;

    for (const auto word : words_) {
        result.set_bits += popcount(word);
    }

    if (shape_.bit_count == 0) {
        return result;
    }

    result.fill_ratio =
        static_cast<double>(result.set_bits) / static_cast<double>(shape_.bit_count);
    result.estimated_false_positive_rate = std::pow(result.fill_ratio, shape_.probe_count);

    if (result.set_bits != shape_.bit_count) {
        result.estimated_items = -static_cast<double>(shape_.bit_count) /
                                 static_cast<double>(shape_.probe_count) *
                                 std::log1p(-result.fill_ratio);
    }

    return result;
}

double BloomFilter::false_positive_rate_for(std::size_t distinct_items) const noexcept {
    if (shape_.bit_count == 0 || distinct_items == 0) {
        return 0;
    }

    return static_cast<double>(
        std::exp(log_false_positive(shape_.bit_count, shape_.probe_count, distinct_items)));
}

void BloomFilter::save(std::ostream& output) const {
    require_active();

    std::array<char, header_size> header{};
    std::copy(magic.begin(), magic.end(), header.begin());
    write_le(header.data() + 8, format_version, 4);
    write_le(header.data() + 12, hash_algorithm, 4);
    write_le(header.data() + 16, static_cast<std::uint64_t>(shape_.bit_count), 8);
    write_le(header.data() + 24, shape_.probe_count, 4);
    write_le(header.data() + 28, shape_.seed, 4);
    write_le(header.data() + 32, static_cast<std::uint64_t>(words_.size()), 8);

    Crc32 checksum;
    checksum.update(header.data(), header.size());
    write_exact(output, header.data(), header.size());

    std::array<char, 4096> buffer{};

    for (std::size_t offset = 0; offset < words_.size();) {
        const auto count = std::min(words_.size() - offset, buffer.size() / 8);

        for (std::size_t i = 0; i < count; ++i) {
            write_le(buffer.data() + i * 8, words_[offset + i], 8);
        }

        checksum.update(buffer.data(), count * 8);
        write_exact(output, buffer.data(), count * 8);
        offset += count;
    }

    std::array<char, 4> trailer{};
    write_le(trailer.data(), checksum.value(), 4);
    write_exact(output, trailer.data(), trailer.size());
}

BloomFilter BloomFilter::load(std::istream& input, std::size_t max_bytes) {
    std::array<char, header_size> header{};
    read_exact(input, header.data(), header.size());

    if (!std::equal(magic.begin(), magic.end(), header.begin()) ||
        read_le(header.data() + 8, 4) != format_version ||
        read_le(header.data() + 12, 4) != hash_algorithm) {
        throw std::runtime_error(
            "Bloom snapshot: unsupported magic, format version or hash algorithm");
    }

    const auto bits = read_le(header.data() + 16, 8);
    const auto probes = read_le(header.data() + 24, 4);
    const auto seed = read_le(header.data() + 28, 4);
    const auto word_count = read_le(header.data() + 32, 8);

    // 在分配内存前验证全部长度，防止损坏的文件头触发超大分配或整数溢出。
    if (bits < 64 || (bits & (bits - 1)) != 0 || probes == 0 || probes > max_probes ||
        word_count != bits / 64 || bits > std::numeric_limits<std::size_t>::max() ||
        bits / 8 > max_bytes) {
        throw std::runtime_error("Bloom snapshot: invalid shape or memory budget exceeded");
    }

    std::vector<std::uint64_t> words(static_cast<std::size_t>(word_count));
    Crc32 checksum;
    checksum.update(header.data(), header.size());
    std::array<char, 4096> buffer{};

    for (std::size_t offset = 0; offset < words.size();) {
        const auto count = std::min(words.size() - offset, buffer.size() / 8);
        read_exact(input, buffer.data(), count * 8);
        checksum.update(buffer.data(), count * 8);

        for (std::size_t i = 0; i < count; ++i) {
            words[offset + i] = read_le(buffer.data() + i * 8, 8);
        }

        offset += count;
    }

    std::array<char, 4> trailer{};
    read_exact(input, trailer.data(), trailer.size());

    if (read_le(trailer.data(), 4) != checksum.value()) {
        throw std::runtime_error("Bloom snapshot: CRC32 mismatch");
    }

    return BloomFilter(Shape{static_cast<std::size_t>(bits), static_cast<std::uint32_t>(probes),
                             static_cast<std::uint32_t>(seed)},
                       std::move(words));
}

void BloomFilter::save(const std::filesystem::path& path) const {
    require_active();
    std::ofstream output(path, std::ios::binary | std::ios::trunc);

    if (!output) {
        throw std::runtime_error("Bloom snapshot: cannot open output file");
    }

    save(output);
    output.close();

    if (!output) {
        throw std::runtime_error("Bloom snapshot: closing output file failed");
    }
}

BloomFilter BloomFilter::load(const std::filesystem::path& path, std::size_t max_bytes) {
    std::ifstream input(path, std::ios::binary);

    if (!input) {
        throw std::runtime_error("Bloom snapshot: cannot open input file");
    }

    auto filter = load(input, max_bytes);

    if (input.peek() != std::char_traits<char>::eof() || input.bad()) {
        throw std::runtime_error("Bloom snapshot: trailing bytes or input error");
    }

    return filter;
}

} // namespace bloom17
