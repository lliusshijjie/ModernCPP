#include "bloom17/bloom_filter.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

int main() {
    try {
        // 用有序数组模拟不可变 SSTable 的精确索引。Bloom filter 在昂贵查询之前工作。
        constexpr std::uint64_t records = 20000;
        constexpr std::uint64_t queries = 100000;
        bloom17::BloomFilter filter(static_cast<std::size_t>(records), 0.01);
        std::vector<std::uint64_t> table;
        table.reserve(static_cast<std::size_t>(records));

        for (std::uint64_t i = 0; i < records; ++i) {
            table.push_back(i * 2);
        }

        filter.insert_range(table.begin(), table.end());

        // 快照模拟把过滤器与 SSTable 一起保存；重新载入后仍无假阴性。
        std::stringstream snapshot(std::ios::in | std::ios::out | std::ios::binary);
        filter.save(snapshot);
        auto restored = bloom17::BloomFilter::load(snapshot);

        std::uint64_t skipped = 0;
        std::uint64_t backend_lookups = 0;
        std::uint64_t hits = 0;
        std::uint64_t false_positives = 0;

        for (std::uint64_t key = 0; key < queries; ++key) {
            if (!restored.possibly_contains_u64(key)) {
                ++skipped;
                continue;
            }

            ++backend_lookups;

            if (std::binary_search(table.begin(), table.end(), key)) {
                ++hits;
            } else {
                ++false_positives;
            }
        }

        if (hits != records || skipped + backend_lookups != queries) {
            throw std::runtime_error("SSTable demo invariant failed");
        }

        const auto stats = restored.statistics();
        const auto negative_queries = queries - records;
        std::cout << std::fixed << std::setprecision(4) << "records=" << records
                  << " queries=" << queries << '\n'
                  << "filter_bytes=" << stats.memory_bytes << " probes=" << stats.probe_count
                  << '\n'
                  << "backend_lookups=" << backend_lookups << " skipped=" << skipped << '\n'
                  << "exact_hits=" << hits << " false_negatives=" << records - hits << '\n'
                  << "false_positives=" << false_positives << " measured_fpr="
                  << static_cast<double>(false_positives) / static_cast<double>(negative_queries)
                  << '\n'
                  << "estimated_fpr=" << stats.estimated_false_positive_rate
                  << " fill_ratio=" << stats.fill_ratio << '\n'
                  << "Demo checks: PASS\n";
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
