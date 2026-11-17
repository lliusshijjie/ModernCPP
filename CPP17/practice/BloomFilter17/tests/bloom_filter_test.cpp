#include "bloom17/bloom_filter.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using bloom17::BloomFilter;
using bloom17::Options;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <class Error = std::exception, class Function>
void expect_throw(Function&& function) {
    bool thrown = false;

    try {
        function();
    } catch (const Error&) {
        thrown = true;
    }

    require(thrown, "expected exception was not thrown");
}

std::string encode(const BloomFilter& filter) {
    std::ostringstream output(std::ios::binary);
    filter.save(output);
    return output.str();
}

BloomFilter decode(const std::string& bytes, std::size_t budget = bloom17::default_max_bytes) {
    std::istringstream input(bytes, std::ios::binary);
    return BloomFilter::load(input, budget);
}

void planning_and_limits() {
    for (const auto count : {1U, 10U, 1000U, 1000000U}) {
        for (const auto rate : {0.5, 0.01, 0.000001}) {
            const Options options{count, rate, 7, bloom17::default_max_bytes};
            const auto shape = BloomFilter::plan(options);
            const BloomFilter filter(options);

            require(shape.bit_count >= 64 && (shape.bit_count & (shape.bit_count - 1)) == 0,
                    "bit count must be a power of two");
            require(shape.probe_count >= 1 && shape.probe_count <= 32, "invalid probe count");
            require(filter.false_positive_rate_for(count) <= rate * (1 + 1e-12),
                    "planned theoretical rate exceeds target");
        }
    }

    expect_throw<std::invalid_argument>([] { BloomFilter filter(0); });

    for (const auto rate : {0.0, -0.1, 1.0, 2.0, std::numeric_limits<double>::infinity(),
                            std::numeric_limits<double>::quiet_NaN()}) {
        expect_throw<std::invalid_argument>([rate] { BloomFilter filter(10, rate); });
    }

    expect_throw<std::length_error>([] { BloomFilter filter(Options{1000, 0.01, 0, 8}); });
    expect_throw<std::length_error>([] { BloomFilter filter(Options{1, 0.1, 0, 7}); });
    expect_throw<std::length_error>([] {
        static_cast<void>(BloomFilter::plan(Options{std::numeric_limits<std::size_t>::max(),
                                                    std::numeric_limits<double>::min(), 0, 1024}));
    });
}

void hash_vectors() {
    // Independent golden vectors for the original MurmurHash3_x64_128, seed = 0.
    const auto empty = bloom17::detail::hash_bytes({}, 0);
    const auto foo = bloom17::detail::hash_bytes("foo", 0);
    const auto hello = bloom17::detail::hash_bytes("hello", 0);

    require(empty.first == 0 && empty.second == 0, "empty hash mismatch");
    require(foo.first == UINT64_C(0xe271865701f54561) && foo.second == UINT64_C(0x7eaf87e42bba7d87),
            "foo hash mismatch");
    require(hello.first == UINT64_C(0xcbd8a7b341bd9b02) &&
                hello.second == UINT64_C(0x5b1e906a48ae1d19),
            "hello hash mismatch");

    std::string long_key(129, '\0');

    for (std::size_t i = 0; i < long_key.size(); ++i) {
        long_key[i] = static_cast<char>((i * 179 + long_key.size()) & 0xffU);
    }

    const auto long_hash = bloom17::detail::hash_bytes(long_key, 42);
    require(long_hash.first == UINT64_C(0x2085b5006502fd2c) &&
                long_hash.second == UINT64_C(0xd6cfe75af8261cf9),
            "long seeded hash mismatch");

    const std::array<char, 8> little_endian{8, 7, 6, 5, 4, 3, 2, 1};
    const auto integer = bloom17::detail::hash_u64(UINT64_C(0x0102030405060708), 42);
    const auto bytes = bloom17::detail::hash_bytes(
        std::string_view(little_endian.data(), little_endian.size()), 42);
    require(integer.first == bytes.first && integer.second == bytes.second,
            "integer encoding must be little endian");
}

void strings_and_binary_keys() {
    BloomFilter filter(1000);
    const auto before = filter.statistics();
    require(!filter.possibly_contains({}), "empty filter unexpectedly matched");
    require(before.set_bits == 0 && before.estimated_items == 0.0, "invalid empty statistics");

    std::vector<std::string> keys{"", "alpha", u8"布隆过滤器", std::string("a\0b", 3)};

    for (std::size_t length = 0; length <= 129; ++length) {
        std::string key(length, '\0');

        for (std::size_t i = 0; i < length; ++i) {
            key[i] = static_cast<char>((i * 179 + length) & 0xffU);
        }

        keys.push_back(std::move(key));
    }

    filter.insert_range(keys.begin(), keys.end());
    require(filter.count_maybe(keys.begin(), keys.end()) == keys.size(), "string false negative");

    const auto once = encode(filter);
    filter.insert_range(keys.begin(), keys.end());
    require(encode(filter) == once, "duplicate inserts must not change bits");

    filter.clear();
    require(filter.statistics().set_bits == 0 && !filter.possibly_contains("alpha"),
            "clear failed");
    filter.insert("reusable");
    require(filter.possibly_contains("reusable"), "clear must preserve reusable capacity");
}

void integers_and_single_pass_range() {
    BloomFilter filter(100000);

    for (std::uint64_t i = 0; i < 100000; ++i) {
        filter.insert_u64(i);
    }

    filter.insert_u64(std::numeric_limits<std::uint64_t>::max());

    for (std::uint64_t i = 0; i < 100000; ++i) {
        require(filter.possibly_contains_u64(i), "integer false negative");
    }

    require(filter.possibly_contains_u64(std::numeric_limits<std::uint64_t>::max()),
            "uint64 max false negative");

    std::istringstream input("11 22 33 44");
    BloomFilter single_pass(10);
    single_pass.insert_range(std::istream_iterator<std::uint64_t>(input),
                             std::istream_iterator<std::uint64_t>());
    const std::array<int, 4> values{11, 22, 33, 44};
    require(single_pass.count_maybe(values.begin(), values.end()) == values.size(),
            "single-pass integer range failed");
}

void empirical_false_positives() {
    constexpr std::uint64_t count = 20000;
    constexpr std::uint64_t queries = 100000;

    for (const auto seed : {0U, bloom17::default_seed, 0xffffffffU}) {
        BloomFilter filter(static_cast<std::size_t>(count), 0.01, seed);

        for (std::uint64_t i = 0; i < count; ++i) {
            filter.insert_u64(i);
        }

        std::uint64_t positives = 0;

        for (std::uint64_t i = count; i < count + queries; ++i) {
            positives += filter.possibly_contains_u64(i) ? 1U : 0U;
        }

        const auto actual = static_cast<double>(positives) / static_cast<double>(queries);
        const auto stats = filter.statistics();
        require(actual < 0.015, "unexpectedly high empirical false positive rate");
        require(std::abs(actual - stats.estimated_false_positive_rate) < 0.004,
                "occupancy estimate differs too far from empirical rate");
        require(stats.estimated_items.has_value() &&
                    std::abs(*stats.estimated_items - static_cast<double>(count)) < 1000,
                "cardinality estimate differs too far");
    }
}

void merge_and_intersection() {
    BloomFilter left(10000);
    BloomFilter right(10000);

    for (std::uint64_t i = 0; i < 1000; ++i) {
        left.insert_u64(i);
        right.insert_u64(i + 500);
    }

    auto united = left;
    united.merge_from(right);
    auto intersection = left;
    intersection.intersect_with(right);

    for (std::uint64_t i = 0; i < 1500; ++i) {
        require(united.possibly_contains_u64(i), "union lost an input key");
    }

    for (std::uint64_t i = 500; i < 1000; ++i) {
        require(intersection.possibly_contains_u64(i), "intersection lost a shared key");
    }

    const auto original = encode(left);
    const BloomFilter same_shape(10000, 0.02);
    require(left.compatible_with(same_shape), "compatible rounded shapes rejected");
    left.merge_from(left);
    left.intersect_with(left);
    require(encode(left) == original, "self merge/intersection changed bits");

    for (const auto options :
         {Options{10000, 0.01, 123, bloom17::default_max_bytes},
          Options{1000000, 0.01, bloom17::default_seed, bloom17::default_max_bytes},
          Options{10000, 0.005, bloom17::default_seed, bloom17::default_max_bytes}}) {
        const BloomFilter incompatible(options);
        require(!left.compatible_with(incompatible), "incompatible filters accepted");
        expect_throw<std::invalid_argument>([&] { left.merge_from(incompatible); });
        expect_throw<std::invalid_argument>([&] { left.intersect_with(incompatible); });
        require(encode(left) == original, "failed merge changed destination");
    }
}

void snapshots_and_stream_framing() {
    BloomFilter original(10000, 0.001, 12345);

    for (std::uint64_t i = 0; i < 10000; ++i) {
        original.insert_u64(i);
    }

    const auto bytes = encode(original);
    auto loaded = decode(bytes);
    require(loaded.compatible_with(original) && encode(loaded) == bytes,
            "snapshot round trip failed");
    require(bytes.size() == 44 + original.statistics().memory_bytes, "unexpected snapshot length");

    for (std::uint64_t i = 0; i < 10000; ++i) {
        require(loaded.possibly_contains_u64(i), "snapshot introduced a false negative");
    }

    std::istringstream concatenated(bytes + bytes, std::ios::binary);
    const auto first = BloomFilter::load(concatenated);
    const auto second = BloomFilter::load(concatenated);
    require(encode(first) == bytes && encode(second) == bytes, "concatenated frames failed");

    expect_throw<std::runtime_error>([&] { static_cast<void>(decode(bytes, 8)); });
}

void invalid_snapshots() {
    BloomFilter tiny(1, 0.5);
    tiny.insert("x");
    const auto bytes = encode(tiny);

    // 对小快照穷举每个截断位置，覆盖头、载荷和校验字段。
    for (std::size_t length = 0; length < bytes.size(); ++length) {
        expect_throw<std::runtime_error>(
            [&] { static_cast<void>(decode(bytes.substr(0, length))); });
    }

    for (std::size_t offset = 0; offset < bytes.size(); ++offset) {
        auto corrupted = bytes;
        corrupted[offset] =
            static_cast<char>(static_cast<unsigned char>(corrupted[offset]) ^ 0x80U);
        expect_throw<std::runtime_error>([&] { static_cast<void>(decode(corrupted)); });
    }

    for (const auto offset : {16U, 24U, 32U}) {
        auto corrupted = bytes;

        for (unsigned i = 0; i < (offset == 24 ? 4U : 8U); ++i) {
            corrupted[offset + i] = '\0';
        }

        expect_throw<std::runtime_error>([&] { static_cast<void>(decode(corrupted)); });
    }

    std::ostringstream failed;
    failed.setstate(std::ios::badbit);
    expect_throw<std::runtime_error>([&] { tiny.save(failed); });
}

struct TempDirectory {
    std::filesystem::path path;

    TempDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();

        for (int attempt = 0; attempt < 100; ++attempt) {
            auto candidate =
                std::filesystem::temp_directory_path() /
                ("bloom17-test-" + std::to_string(stamp) + "-" + std::to_string(attempt));

            if (std::filesystem::create_directory(candidate)) {
                path = std::move(candidate);
                return;
            }
        }

        throw std::runtime_error("cannot create an exclusive test directory");
    }

    ~TempDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

void file_persistence() {
    const TempDirectory temporary;
    const auto path = temporary.path / "index.bloom";
    const auto missing = temporary.path / "missing" / "index.bloom";
    BloomFilter filter(100);
    filter.insert("persisted");
    filter.save(path);

    const auto loaded = BloomFilter::load(path);
    require(loaded.possibly_contains("persisted"), "file persistence failed");
    expect_throw<std::runtime_error>([&] { filter.save(missing); });
    expect_throw<std::runtime_error>([&] { static_cast<void>(BloomFilter::load(missing)); });

    {
        std::ofstream append(path, std::ios::binary | std::ios::app);
        append.put('!');
    }

    expect_throw<std::runtime_error>([&] { static_cast<void>(BloomFilter::load(path)); });
}

void copy_move_and_swap() {
    BloomFilter source(100);
    source.insert("alpha");
    auto copy = source;
    copy.clear();
    require(source.possibly_contains("alpha") && !copy.possibly_contains("alpha"),
            "copy aliases data");

    BloomFilter assigned(100000);
    assigned = source;
    require(encode(assigned) == encode(source), "copy assignment failed");

    auto moved = std::move(source);
    require(moved.possibly_contains("alpha") && !source.possibly_contains("alpha"), "move failed");
    require(source.statistics().bit_count == 0, "moved-from statistics invalid");
    expect_throw<std::logic_error>([&] { source.insert("x"); });
    expect_throw<std::logic_error>([&] { static_cast<void>(encode(source)); });

    source = std::move(moved);
    require(source.possibly_contains("alpha") && !moved.possibly_contains("alpha"),
            "move assignment failed");
    source.swap(copy);
    require(copy.possibly_contains("alpha") && !source.possibly_contains("alpha"), "swap failed");
}

void saturation() {
    BloomFilter filter(1, 0.5);

    for (std::uint64_t i = 0; i < 10000; ++i) {
        filter.insert_u64(i);
    }

    const auto stats = filter.statistics();
    require(stats.fill_ratio == 1 && stats.estimated_false_positive_rate == 1,
            "saturated filter statistics invalid");
    require(!stats.estimated_items.has_value(), "saturated cardinality must be unknown");
    require(filter.possibly_contains("anything"), "saturated filter should match every key");
    filter.clear();
    require(filter.statistics().estimated_items == 0.0, "clear did not reset saturated filter");
}

void parallel_readers() {
    BloomFilter filter(20000);

    for (std::uint64_t i = 0; i < 20000; ++i) {
        filter.insert_u64(i);
    }

    const auto& published = filter;
    std::vector<std::future<bool>> readers;

    for (int worker = 0; worker < 4; ++worker) {
        readers.push_back(std::async(std::launch::async, [&published] {
            for (std::uint64_t i = 0; i < 20000; ++i) {
                if (!published.possibly_contains_u64(i)) {
                    return false;
                }
            }

            return true;
        }));
    }

    for (auto& reader : readers) {
        require(reader.get(), "parallel reader saw a false negative");
    }
}

} // namespace

int main() {
    using Test = std::pair<const char*, void (*)()>;
    const std::array tests{
        Test{"planning_and_limits", planning_and_limits},
        Test{"hash_vectors", hash_vectors},
        Test{"strings_and_binary_keys", strings_and_binary_keys},
        Test{"integers_and_single_pass_range", integers_and_single_pass_range},
        Test{"empirical_false_positives", empirical_false_positives},
        Test{"merge_and_intersection", merge_and_intersection},
        Test{"snapshots_and_stream_framing", snapshots_and_stream_framing},
        Test{"invalid_snapshots", invalid_snapshots},
        Test{"file_persistence", file_persistence},
        Test{"copy_move_and_swap", copy_move_and_swap},
        Test{"saturation", saturation},
        Test{"parallel_readers", parallel_readers},
    };

    std::size_t passed = 0;

    for (const auto& [name, run] : tests) {
        try {
            run();
            ++passed;
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        }
    }

    std::cout << "Tests: " << passed << '/' << tests.size() << " passed\n";
    return passed == tests.size() ? 0 : 1;
}
