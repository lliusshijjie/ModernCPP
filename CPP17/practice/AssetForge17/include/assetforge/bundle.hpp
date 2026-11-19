#pragma once
#include "catalog.hpp"
#include <fstream>
#include <vector>

namespace assetforge {
using Bytes = std::vector<std::byte>;
enum class Policy { seq, par };
inline constexpr std::string_view magic = "AFP1"; // [26] Same entity across translation units.
inline constexpr std::uint16_t version = 1;
inline constexpr Size header_size = 32;

struct Entry {
    std::string id;
    Size offset, size;
    std::uint32_t crc;
    std::vector<Index> dependencies;
};

class BundleReader {
public:
    [[nodiscard]] static BundleReader open(const fs::path& file);
    BundleReader(const BundleReader&) = delete;
    BundleReader(BundleReader&&) = delete;

    const std::vector<Entry>& entries() const noexcept {
        return entries_;
    }

    const Names& aliases() const noexcept {
        return aliases_;
    }

    Size content_size() const noexcept {
        return content_size_;
    }

    [[nodiscard]] std::optional<Index> find(std::string_view name) const;
    [[nodiscard]] std::optional<Bytes> read(std::string_view name);
    [[nodiscard]] std::vector<std::string> verify(Policy policy = Policy::seq);

private:
    explicit BundleReader(const fs::path& file);
    Bytes read_at(Size offset, Size count);

    std::ifstream stream_;
    std::vector<Entry> entries_;
    Names ids_, aliases_;
    Size content_base_{}, content_size_{};
};

struct AssetMetric {
    Size size;
    Size replacements;
};

struct BuildSummary {
    std::vector<AssetMetric> metrics;
    Size content_size{};
    std::size_t alias_count{};
};

struct BuildError {
    std::string message;
};

using BuildResult = std::variant<BuildSummary, BuildError>;

[[nodiscard]] BuildResult build(const BuildPlan&, const fs::path&, Policy = Policy::seq);
} // namespace assetforge
