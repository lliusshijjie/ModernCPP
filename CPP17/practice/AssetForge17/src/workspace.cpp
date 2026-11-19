#include "assetforge/catalog.hpp"
#include <mutex>
#include <type_traits>
#include <utility>

namespace assetforge {
Workspace::Workspace(Catalog catalog, Profile profile)
    : catalog_(std::move(catalog)), profile_(std::move(profile)) {
    catalog_.profile(profile_.name);
}

WorkspaceSnapshot Workspace::snapshot() const {
    // [23] Both locks must be acquired together, including on the read path.
    std::shared_lock catalog_lock(catalog_mutex_, std::defer_lock);
    std::shared_lock selection_lock(selection_mutex_, std::defer_lock);
    std::lock(catalog_lock, selection_lock);
    return {catalog_, profile_};
}

std::optional<AssetSpec> Workspace::find(std::string_view name) const {
    std::shared_lock lock(catalog_mutex_);
    if (auto id = catalog_.find(name)) {
        return catalog_.assets_[*id];
    }
    return std::nullopt;
}

Profile Workspace::selected() const {
    std::shared_lock lock(selection_mutex_);
    return profile_;
}

void Workspace::select(Profile profile) {
    std::scoped_lock lock(catalog_mutex_, selection_mutex_);
    catalog_.profile(profile.name);
    profile_.name.swap(profile.name);
}

void Workspace::reload(const fs::path& manifest) {
    auto candidate = load_catalog(manifest);
    std::scoped_lock lock(catalog_mutex_, selection_mutex_);
    candidate.profile(profile_.name);

    static_assert(std::is_nothrow_move_assignable_v<Catalog>);
    catalog_ = std::move(candidate);
}

void Workspace::rename_alias(std::string_view old_name, std::string new_name) {
    if (!valid_name(new_name)) {
        throw Error("invalid alias");
    }
    std::unique_lock lock(catalog_mutex_);
    auto& aliases = catalog_.aliases_;
    auto it = aliases.find(old_name);

    if (it == aliases.end()) {
        throw Error("unknown alias: " + std::string(old_name));
    }
    if (it->first == new_name) {
        return;
    }
    if (catalog_.ids_.find(new_name) != catalog_.ids_.end()) {
        throw Error("name is an asset ID");
    }

    // [30] Pre-allocated keys and the same map allocator make rollback allocation-free.
    auto node = aliases.extract(it);
    node.key().swap(new_name);
    auto result = aliases.insert(std::move(node));
    if (!result.inserted) {
        result.node.key().swap(new_name);
        aliases.insert(std::move(result.node));
        throw Error("alias already exists");
    }
}
} // namespace assetforge
