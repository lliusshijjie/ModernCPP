#pragma once

#include "detail/hash.hpp"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

namespace bloom17 {

static_assert(CHAR_BIT == 8, "BloomFilter17 requires 8-bit bytes");
static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t), "Unsupported address width");

inline constexpr std::size_t default_max_bytes = 256U * 1024U * 1024U;
inline constexpr std::uint32_t default_seed = 0x9747b28cU;

struct Options {
    std::size_t expected_items = 10000;
    double false_positive_rate = 0.01;
    std::uint32_t seed = default_seed;
    std::size_t max_bytes = default_max_bytes;
};

struct Shape {
    std::size_t bit_count = 0;
    std::uint32_t probe_count = 0;
    std::uint32_t seed = 0;
};

struct Statistics {
    std::size_t bit_count = 0;
    std::size_t memory_bytes = 0;
    std::size_t set_bits = 0;
    std::uint32_t probe_count = 0;
    double fill_ratio = 0;
    double estimated_false_positive_rate = 0;

    // 空过滤器为 0；完全饱和时无法再估计，返回 nullopt。
    std::optional<double> estimated_items;
};

// 普通 Bloom filter：并发只读安全；任何写入与其他访问之间都需要外部同步。
// false = 一定不存在；true = 可能存在，调用方仍须访问真实数据源确认。
class BloomFilter {
public:
    explicit BloomFilter(const Options& options = {});

    explicit BloomFilter(std::size_t expected_items, double false_positive_rate = 0.01,
                         std::uint32_t seed = default_seed);

    BloomFilter(const BloomFilter&) = default;

    BloomFilter& operator=(const BloomFilter& other);

    BloomFilter(BloomFilter&& other) noexcept;

    BloomFilter& operator=(BloomFilter&& other) noexcept;

    // 只规划参数，不分配位数组。选用满足近似目标的最少探测次数，最多 32 次。
    [[nodiscard]] static Shape plan(const Options& options);

    void insert(std::string_view key) {
        insert_hash(detail::hash_bytes(key, shape_.seed));
    }

    [[nodiscard]] bool possibly_contains(std::string_view key) const noexcept {
        return contains_hash(detail::hash_bytes(key, shape_.seed));
    }

    // 整数统一编码为 8 字节小端；不使用平台相关的 std::hash。
    void insert_u64(std::uint64_t key) {
        insert_hash(detail::hash_u64(key, shape_.seed));
    }

    [[nodiscard]] bool possibly_contains_u64(std::uint64_t key) const noexcept {
        return contains_hash(detail::hash_u64(key, shape_.seed));
    }

    // 支持字符串范围和整数范围，单遍迭代、不复制键、不分配临时容器。
    template <class InputIt>
    void insert_range(InputIt first, InputIt last) {
        for (; first != last; ++first) {
            if constexpr (std::is_integral_v<std::decay_t<decltype(*first)>>) {
                insert_u64(static_cast<std::uint64_t>(*first));
            } else {
                insert(std::string_view(*first));
            }
        }
    }

    template <class InputIt>
    [[nodiscard]] std::size_t count_maybe(InputIt first, InputIt last) const {
        std::size_t count = 0;

        for (; first != last; ++first) {
            if constexpr (std::is_integral_v<std::decay_t<decltype(*first)>>) {
                count += possibly_contains_u64(static_cast<std::uint64_t>(*first)) ? 1U : 0U;
            } else {
                count += possibly_contains(std::string_view(*first)) ? 1U : 0U;
            }
        }

        return count;
    }

    void clear() noexcept;

    void swap(BloomFilter& other) noexcept;

    [[nodiscard]] Shape shape() const noexcept {
        return shape_;
    }

    [[nodiscard]] bool compatible_with(const BloomFilter& other) const noexcept;

    // 按位 OR 构造并集；不兼容时抛异常，原对象保持不变。
    void merge_from(const BloomFilter& other);

    // 按位 AND：保留真实交集的所有元素，但可能有额外阳性结果。
    // 不能用结果推断两个原始集合完全相等或交集的精确大小。
    void intersect_with(const BloomFilter& other);

    // 以下统计为 O(位数组长度)，放在热路径外按需调用。
    [[nodiscard]] Statistics statistics() const noexcept;

    [[nodiscard]] double false_positive_rate_for(std::size_t distinct_items) const noexcept;

    // 版本化、小端序、CRC32 快照；stream 重载恰好读/写一帧，允许拼接。
    void save(std::ostream& output) const;

    void save(const std::filesystem::path& path) const;

    [[nodiscard]] static BloomFilter load(std::istream& input,
                                          std::size_t max_bytes = default_max_bytes);

    [[nodiscard]] static BloomFilter load(const std::filesystem::path& path,
                                          std::size_t max_bytes = default_max_bytes);

private:
    BloomFilter(Shape shape, std::vector<std::uint64_t> words) noexcept;

    void require_active() const;

    void require_compatible(const BloomFilter& other) const;

    void insert_hash(detail::Hash128 hash) {
        require_active();

        const auto mask = static_cast<std::uint64_t>(shape_.bit_count - 1);
        const auto step = hash.second | UINT64_C(1);
        auto position = hash.first;

        for (std::uint32_t i = 0; i < shape_.probe_count; ++i) {
            const auto bit = position & mask;
            words_[static_cast<std::size_t>(bit >> 6U)] |= UINT64_C(1) << (bit & 63U);
            position += step;
        }
    }

    [[nodiscard]] bool contains_hash(detail::Hash128 hash) const noexcept {
        if (shape_.bit_count == 0) {
            return false;
        }

        const auto mask = static_cast<std::uint64_t>(shape_.bit_count - 1);
        const auto step = hash.second | UINT64_C(1);
        auto position = hash.first;

        for (std::uint32_t i = 0; i < shape_.probe_count; ++i) {
            const auto bit = position & mask;

            if ((words_[static_cast<std::size_t>(bit >> 6U)] & (UINT64_C(1) << (bit & 63U))) == 0) {
                return false;
            }

            position += step;
        }

        return true;
    }

    Shape shape_;
    std::vector<std::uint64_t> words_;
};

} // namespace bloom17
