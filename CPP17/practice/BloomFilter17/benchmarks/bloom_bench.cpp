#include "bloom17/bloom_filter.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

// 使每轮查询的结果可观测；计时区间内不打印、不生成键。
volatile std::uint64_t result_sink = 0;

struct Settings {
    std::size_t items = 200000;
    std::size_t queries = 500000;
    std::size_t rounds = 5;
    bool help = false;
};

std::size_t parse_size(std::string_view input, std::size_t maximum) {
    std::size_t result = 0;
    const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), result);

    if (error != std::errc{} || end != input.data() + input.size() || result == 0 ||
        result > maximum) {
        throw std::invalid_argument("benchmark argument is outside its supported positive range");
    }

    return result;
}

Settings parse_arguments(int argc, char** argv) {
    Settings settings;

    for (int i = 1; i < argc; ++i) {
        const std::string_view option(argv[i]);

        if (option == "--help") {
            settings.help = true;
            return settings;
        }

        if (i + 1 >= argc) {
            throw std::invalid_argument("missing benchmark argument value");
        }

        const std::string_view value(argv[++i]);

        if (option == "--items") {
            settings.items = parse_size(value, 2000000);
        } else if (option == "--queries") {
            settings.queries = parse_size(value, 2000000);
        } else if (option == "--rounds") {
            settings.rounds = parse_size(value, 20);
        } else {
            throw std::invalid_argument("unknown benchmark argument");
        }
    }

    return settings;
}

template <class Setup, class Work>
void measure(std::string_view name, std::size_t operations, std::size_t rounds, Setup&& setup,
             Work&& work) {
    setup();
    result_sink = result_sink + static_cast<std::uint64_t>(work()); // 一轮预热
    std::vector<double> samples;
    samples.reserve(rounds);

    for (std::size_t round = 0; round < rounds; ++round) {
        setup();
        const auto start = Clock::now();
        const auto result = work();
        const auto finish = Clock::now();

        result_sink = result_sink + static_cast<std::uint64_t>(result);
        samples.push_back(std::chrono::duration<double>(finish - start).count());
    }

    std::sort(samples.begin(), samples.end());
    const auto seconds = samples[samples.size() / 2];
    const auto throughput = static_cast<double>(operations) / seconds / 1e6;
    const auto nanoseconds = seconds * 1e9 / static_cast<double>(operations);

    std::cout << std::left << std::setw(29) << name << std::right << std::setw(12) << throughput
              << " Mops/s" << std::setw(12) << nanoseconds << " ns/op\n";
}

template <class Range, class Predicate>
std::size_t count_matches(const Range& keys, Predicate&& predicate) {
    std::size_t count = 0;

    for (const auto& key : keys) {
        count += predicate(key) ? 1U : 0U;
    }

    return count;
}

void run(const Settings& settings) {
    std::mt19937_64 random(20261116);
    std::vector<std::uint64_t> inserted(settings.items);
    std::vector<std::uint64_t> present(settings.queries);
    std::vector<std::uint64_t> absent(settings.queries);
    std::vector<std::uint64_t> mixed(settings.queries);

    for (std::size_t i = 0; i < settings.items; ++i) {
        inserted[i] = static_cast<std::uint64_t>(i);
    }

    std::shuffle(inserted.begin(), inserted.end(), random);

    for (std::size_t i = 0; i < settings.queries; ++i) {
        present[i] = random() % settings.items;
        absent[i] = static_cast<std::uint64_t>(settings.items) + random() % settings.items;
        mixed[i] = i % 2 == 0 ? present[i] : absent[i];
    }

    bloom17::BloomFilter filter(settings.items, 0.01);
    std::unordered_set<std::uint64_t> exact;
    exact.reserve(settings.items);
    exact.insert(inserted.begin(), inserted.end());

    std::cout << std::fixed << std::setprecision(2) << "items=" << settings.items
              << " queries=" << settings.queries << " rounds=" << settings.rounds
              << " target_fpr=0.01\n"
              << "Median after one warm-up; key generation and setup excluded.\n";

    measure(
        "bloom u64 insert", settings.items, settings.rounds, [&] { filter.clear(); },
        [&] {
            filter.insert_range(inserted.begin(), inserted.end());
            return settings.items;
        });

    const auto no_setup = [] {};
    const auto lookup = [&filter](std::uint64_t key) { return filter.possibly_contains_u64(key); };

    measure("bloom u64 hit", settings.queries, settings.rounds, no_setup,
            [&] { return count_matches(present, lookup); });
    measure("bloom u64 miss", settings.queries, settings.rounds, no_setup,
            [&] { return count_matches(absent, lookup); });
    measure("bloom u64 mixed (50% hit)", settings.queries, settings.rounds, no_setup,
            [&] { return count_matches(mixed, lookup); });

    measure("unordered_set u64 miss", settings.queries, settings.rounds, no_setup, [&] {
        return count_matches(absent, [&exact](std::uint64_t key) { return exact.count(key) != 0; });
    });

    measure("bloom + exact u64 miss", settings.queries, settings.rounds, no_setup, [&] {
        return count_matches(absent, [&](std::uint64_t key) {
            return filter.possibly_contains_u64(key) && exact.count(key) != 0;
        });
    });

    const auto stats = filter.statistics();
    const auto false_positives = count_matches(absent, lookup);
    std::cout << "filter_bytes=" << stats.memory_bytes << " probes=" << stats.probe_count
              << " measured_fpr=" << std::setprecision(6)
              << static_cast<double>(false_positives) / static_cast<double>(settings.queries)
              << " estimated_fpr=" << stats.estimated_false_positive_rate << '\n';

    // 字符串测量同时包含字符串哈希成本，键已预先生成。
    std::vector<std::string> string_keys;
    std::vector<std::string> string_absent;
    string_keys.reserve(settings.items);
    string_absent.reserve(settings.queries);

    for (const auto key : inserted) {
        string_keys.push_back("customer/region-cn/record/" + std::to_string(key));
    }

    for (const auto key : absent) {
        string_absent.push_back("customer/region-cn/record/" + std::to_string(key));
    }

    bloom17::BloomFilter strings(settings.items, 0.01);
    std::cout << std::setprecision(2);

    measure(
        "bloom string insert", settings.items, settings.rounds, [&] { strings.clear(); },
        [&] {
            strings.insert_range(string_keys.begin(), string_keys.end());
            return settings.items;
        });

    measure("bloom string miss", settings.queries, settings.rounds, no_setup,
            [&] { return strings.count_maybe(string_absent.begin(), string_absent.end()); });

    std::cout << "sink=" << result_sink << '\n';
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto settings = parse_arguments(argc, argv);

        if (settings.help) {
            std::cout << "Usage: bloom_bench [--items 1..2000000] [--queries 1..2000000] "
                         "[--rounds 1..20]\n";
            return 0;
        }

        run(settings);
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
