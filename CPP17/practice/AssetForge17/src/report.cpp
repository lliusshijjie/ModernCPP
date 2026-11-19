#include "assetforge/report.hpp"
#include <array>
#include <charconv>

namespace assetforge {
namespace {
struct Normalization {
    Size changed_assets{}, replacements{};

    std::string display() const {
        return "normalized_assets=" + decimal(changed_assets) +
               " crlf_pairs=" + decimal(replacements);
    }
};

struct Histogram {
    std::array<Size, 4> counts{};

    std::string display() const {
        return "size_histogram empty=" + decimal(counts[0]) +
               " small_1_1024=" + decimal(counts[1]) + " medium_1025_65536=" + decimal(counts[2]) +
               " large=" + decimal(counts[3]);
    }
};

Normalization normalization(const BuildSummary& summary) {
    Normalization result;
    for (const auto& metric : summary.metrics) {
        if (metric.replacements) {
            ++result.changed_assets;
        }
        result.replacements += metric.replacements;
    }
    return result;
}

Histogram histogram(const BuildSummary& summary) {
    Histogram result;
    for (const auto& metric : summary.metrics) {
        auto group = metric.size == 0 ? 0 : metric.size <= 1024 ? 1 : metric.size <= 65536 ? 2 : 3;
        ++result.counts[group];
    }
    return result;
}
} // namespace

bool ReportRegistry::contains(std::string_view name) const {
    return entries_.find(name) != entries_.end();
}

std::string ReportRegistry::render(std::string_view name, const BuildSummary& summary) const {
    if (auto it = entries_.find(name); it != entries_.end()) {
        auto payload = it->second.collect(summary);
        return it->second.render(payload);
    }
    throw Error("unknown report: " + std::string(name));
}

ReportRegistry default_reports() {
    ReportRegistry registry;
    registry.add("normalization", normalization, &Normalization::display); // [29]
    registry.add("sizes", histogram, &Histogram::display);
    return registry;
}

std::string decimal(Size value) {
    std::array<char, 20> text;
    auto [end, error] = std::to_chars(text.data(), text.data() + text.size(), value); // [24]
    if (error != std::errc{}) {
        throw std::logic_error("unsigned decimal buffer is too small");
    }
    return std::string(text.data(), end);
}
} // namespace assetforge
