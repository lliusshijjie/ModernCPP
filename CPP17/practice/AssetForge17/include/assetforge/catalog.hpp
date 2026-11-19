#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace assetforge {
namespace fs = std::filesystem;
using Index = std::uint32_t;
using Size = std::uint64_t;
using Names = std::map<std::string, Index, std::less<>>;

inline constexpr std::size_t max_assets = 4096;
inline constexpr std::size_t max_edges = 16384;
inline constexpr std::size_t max_aliases = 4096;
inline constexpr Size max_file = 8 * 1024 * 1024;
inline constexpr Size max_content = 64 * 1024 * 1024;
inline constexpr Size max_index = 2 * 1024 * 1024;

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct RawSource {};

struct TextLfSource {};

using SourceMode = std::variant<RawSource, TextLfSource>;

struct AssetSpec {
    std::string id;
    SourceMode mode;
    fs::path path;
    std::optional<Size> limit;
};

struct Edge {
    Index target;
    std::size_t line;
};

struct Profile {
    std::string name;
};

struct Root {
    std::string name;
};

using Selection = std::variant<Profile, Root>;

class Catalog {
public:
    const fs::path& directory() const noexcept {
        return directory_;
    }

    const std::vector<AssetSpec>& assets() const noexcept {
        return assets_;
    }

    const std::vector<std::vector<Edge>>& dependencies() const noexcept {
        return deps_;
    }

    const Names& aliases() const noexcept {
        return aliases_;
    }

    std::optional<Index> find(std::string_view name) const;
    const std::vector<Index>& profile(std::string_view name) const;

private:
    friend Catalog load_catalog(const fs::path&);
    friend class Workspace;

    fs::path directory_;
    std::vector<AssetSpec> assets_;
    std::vector<std::vector<Edge>> deps_;
    Names ids_, aliases_;
    std::map<std::string, std::vector<Index>, std::less<>> profiles_;
};

struct PlannedAsset {
    AssetSpec spec;
    std::vector<Index> dependencies;
};

class BuildPlan {
public:
    const fs::path& directory() const noexcept {
        return directory_;
    }

    const std::vector<PlannedAsset>& assets() const noexcept {
        return assets_;
    }

    const Names& aliases() const noexcept {
        return aliases_;
    }

private:
    friend BuildPlan plan(const Catalog&, const Selection&);
    BuildPlan() = default;

    fs::path directory_;
    std::vector<PlannedAsset> assets_;
    Names aliases_;
};

struct WorkspaceSnapshot {
    Catalog catalog;
    Profile profile;
};

class Workspace {
public:
    Workspace(Catalog catalog, Profile profile);
    [[nodiscard]] WorkspaceSnapshot snapshot() const;
    [[nodiscard]] std::optional<AssetSpec> find(std::string_view name) const;
    [[nodiscard]] Profile selected() const;
    void select(Profile profile);
    void reload(const fs::path& manifest);
    void rename_alias(std::string_view old_name, std::string new_name);

private:
    mutable std::shared_mutex catalog_mutex_;
    mutable std::shared_mutex selection_mutex_;
    Catalog catalog_;
    Profile profile_;
};

[[nodiscard]] Catalog load_catalog(const fs::path& manifest);
[[nodiscard]] BuildPlan plan(const Catalog& catalog, const Selection& selection);
bool valid_name(std::string_view name) noexcept;
} // namespace assetforge
