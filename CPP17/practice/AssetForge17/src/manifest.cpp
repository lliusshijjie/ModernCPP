#include "detail.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <set>
#include <utility>

namespace assetforge {
namespace {
constexpr Size max_manifest = 2 * 1024 * 1024;
constexpr std::size_t max_line = 4096;

struct AssetDecl {
    AssetSpec spec;
};

struct RequireDecl {
    std::string owner, target;
};

struct AliasDecl {
    std::string name, target;
};

struct ProfileDecl {
    std::string name, roots;
};

using ManifestRecord = std::variant<AssetDecl, RequireDecl, AliasDecl, ProfileDecl>;
enum class Directive { asset, requires_asset, alias, profile };

Directive directive(std::string_view field) {
    if (field == "asset") {
        return Directive::asset;
    }
    if (field == "requires") {
        return Directive::requires_asset;
    }
    if (field == "alias") {
        return Directive::alias;
    }
    if (field == "profile") {
        return Directive::profile;
    }
    throw Error("unknown directive");
}

bool utf8(std::string_view text) {
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i++]);
        if (lead < 0x80) {
            continue;
        }
        if (lead < 0xc2 || lead > 0xf4) {
            return false;
        }

        unsigned extra = lead < 0xe0 ? 1 : lead < 0xf0 ? 2 : 3;
        if (text.size() - i < extra) {
            return false;
        }
        const auto first = static_cast<unsigned char>(text[i]);
        if ((lead == 0xe0 && first < 0xa0) || (lead == 0xed && first >= 0xa0) ||
            (lead == 0xf0 && first < 0x90) || (lead == 0xf4 && first >= 0x90)) {
            return false;
        }
        while (extra--) {
            if ((static_cast<unsigned char>(text[i++]) & 0xc0) != 0x80) {
                return false;
            }
        }
    }
    return true;
}

std::string_view take(std::string_view& text, char delimiter) {
    auto end = text.find(delimiter);
    auto field = text.substr(0, end);
    text.remove_prefix(end == text.npos ? text.size() : end + 1);
    return field;
}

template <class... Check>
void validate(Check&&... check) {
    // [14] A failed check throws, so fields after it are not evaluated.
    (std::forward<Check>(check)(), ...);
}

void require_name(std::string_view name, std::string_view field) {
    if (!valid_name(name)) {
        throw Error("invalid " + std::string(field));
    }
}

void check_path(std::string_view path) {
    if (path.empty() || path.find_first_of("\\:") != path.npos) {
        throw Error("expected a relative resource path with '/' separators");
    }
    for (;;) {
        auto end = path.find('/');
        auto part = path.substr(0, end);
        if (part.empty() || part == "." || part == "..") {
            throw Error("invalid resource path component");
        }
        if (end == path.npos) {
            return;
        }
        path.remove_prefix(end + 1);
    }
}

std::optional<Size> parse_limit(std::string_view field) {
    if (field == "-") {
        return std::nullopt;
    }
    if (field.empty()) {
        throw Error("empty byte limit");
    }

    Size limit{};
    auto [end, status] = std::from_chars(field.data(), field.data() + field.size(), limit); // [24]
    if (status != std::errc{} || end != field.data() + field.size() || limit > max_file) {
        throw Error("invalid byte limit (expected 0..8388608 or '-')");
    }
    return limit;
}

ManifestRecord parse_line(std::string_view text) {
    std::array<std::string_view, 5> fields;
    std::size_t count = 0;
    for (;;) {
        if (count == fields.size()) {
            throw Error("too many fields");
        }
        auto end = text.find('\t');
        fields[count++] = take(text, '\t'); // [20] Views live only while the input buffer is owned.
        if (end == std::string_view::npos) {
            break;
        }
    }
    auto expect = [&](std::size_t n) {
        if (count != n) {
            throw Error("wrong field count");
        }
    };

    switch (auto kind = directive(fields[0]); kind) { // [12]
    case Directive::asset: {
        expect(5);
        SourceMode mode;
        std::optional<Size> limit;
        validate([&] { require_name(fields[1], "asset ID"); },
                 [&] {
                     if (fields[2] == "text-lf") {
                         mode = TextLfSource{};
                     } else if (fields[2] != "raw") {
                         throw Error("unknown asset mode");
                     }
                 },
                 [&] { check_path(fields[3]); }, [&] { limit = parse_limit(fields[4]); });
        return AssetDecl{{std::string(fields[1]), mode, fs::u8path(fields[3]), limit}};
    }
    case Directive::requires_asset:
        expect(3);
        validate([&] { require_name(fields[1], "asset ID"); },
                 [&] { require_name(fields[2], "dependency ID"); });
        return RequireDecl{std::string(fields[1]), std::string(fields[2])};
    case Directive::alias:
        expect(3);
        validate([&] { require_name(fields[1], "alias"); },
                 [&] { require_name(fields[2], "target ID"); });
        return AliasDecl{std::string(fields[1]), std::string(fields[2])};
    case Directive::profile:
        expect(3);
        require_name(fields[1], "profile");
        return ProfileDecl{std::string(fields[1]), std::string(fields[2])};
    }

    throw Error("invalid directive");
}
} // namespace

bool valid_name(std::string_view name) noexcept {
    return !name.empty() && name.size() <= 64 && std::all_of(name.begin(), name.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    });
}

std::string detail::at_line(std::size_t line, std::string_view reason) {
    return "line " + std::to_string(line) + ": " + std::string(reason);
}

std::optional<Index> Catalog::find(std::string_view name) const {
    if (auto it = ids_.find(name); it != ids_.end()) {
        return it->second;
    }
    if (auto it = aliases_.find(name); it != aliases_.end()) {
        return it->second;
    }
    return std::nullopt;
}

const std::vector<Index>& Catalog::profile(std::string_view name) const {
    if (auto it = profiles_.find(name); it != profiles_.end()) {
        return it->second;
    }
    throw Error("unknown profile: " + std::string(name));
}

Catalog load_catalog(const fs::path& manifest) {
    Catalog catalog;
    auto resolved = fs::canonical(manifest);
    catalog.directory_ = resolved.parent_path();

    auto bytes = detail::read_file(resolved, max_manifest);
    if (bytes.empty()) {
        throw Error("empty manifest");
    }
    std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    if (text.substr(0, 3) == "\xef\xbb\xbf") {
        text.remove_prefix(3);
    }

    using Declaration = std::pair<std::size_t, ManifestRecord>;
    std::vector<Declaration> deferred;
    std::size_t edge_count = 0, alias_count = 0, profile_count = 0;

    for (std::size_t line = 1; !text.empty(); ++line) {
        auto record = take(text, '\n');
        if (!record.empty() && record.back() == '\r') {
            record.remove_suffix(1);
        }
        if (record.size() > max_line) {
            throw Error(detail::at_line(line, "line too long"));
        }
        if (record.empty() || record.front() == '#') {
            continue;
        }

        try {
            if (!utf8(record) || record.find('\0') != record.npos ||
                record.find('\r') != record.npos) {
                throw Error("invalid UTF-8 or control delimiter");
            }

            auto declaration = parse_line(record);
            if (auto asset = std::get_if<AssetDecl>(&declaration)) {
                const auto ordinal = static_cast<Index>(catalog.assets_.size());
                auto [it, inserted] = catalog.ids_.try_emplace(asset->spec.id, ordinal); // [11,30]
                if (!inserted) {
                    throw Error("duplicate asset: " + it->first);
                }
                if (ordinal >= max_assets) {
                    throw Error("asset limit exceeded");
                }
                catalog.assets_.push_back(std::move(asset->spec));
            } else {
                if ((std::holds_alternative<RequireDecl>(declaration) &&
                     ++edge_count > max_edges) ||
                    (std::holds_alternative<AliasDecl>(declaration) &&
                     ++alias_count > max_aliases) ||
                    (std::holds_alternative<ProfileDecl>(declaration) &&
                     ++profile_count > max_assets)) {
                    throw Error("declaration limit exceeded");
                }
                deferred.emplace_back(line, std::move(declaration));
            }
        } catch (const Error& error) {
            throw Error(detail::at_line(line, error.what()));
        }
    }

    if (catalog.assets_.empty()) {
        throw Error("manifest declares no assets");
    }
    catalog.deps_.resize(catalog.assets_.size());

    auto asset_id = [&](std::string_view id) {
        if (auto it = catalog.ids_.find(id); it != catalog.ids_.end()) {
            return it->second;
        }
        throw Error("unknown asset: " + std::string(id));
    };

    std::set<std::pair<Index, Index>> edges_seen;
    for (const auto& [line, declaration] : deferred) {
        try {
            // [17] Resolve names only after all stable IDs are present.
            std::visit(detail::Overloaded{
                           [](const AssetDecl&) {
                               throw std::logic_error("asset in deferred declarations");
                           },
                           [&](const RequireDecl& edge) {
                               auto owner = asset_id(edge.owner);
                               auto target = asset_id(edge.target);
                               if (!edges_seen.emplace(owner, target).second) {
                                   throw Error("duplicate dependency");
                               }
                               catalog.deps_[owner].push_back({target, line});
                           },
                           [&](const AliasDecl& alias) {
                               if (catalog.find(alias.name)) {
                                   throw Error("duplicate name: " + alias.name);
                               }
                               catalog.aliases_.try_emplace(alias.name, asset_id(alias.target));
                           },
                           [&](const ProfileDecl& profile) {
                               auto [it, inserted] = catalog.profiles_.try_emplace(profile.name);
                               if (!inserted) {
                                   throw Error("duplicate profile: " + profile.name);
                               }
                               auto& roots = it->second;
                               std::vector<bool> seen(catalog.assets_.size());
                               std::string_view rest(profile.roots);
                               for (;;) {
                                   auto comma = rest.find(',');
                                   auto root = take(rest, ',');
                                   require_name(root, "profile root");
                                   auto id = asset_id(root);
                                   if (seen[id]) {
                                       throw Error("duplicate profile root");
                                   }
                                   seen[id] = true;
                                   roots.push_back(id);
                                   if (comma == std::string_view::npos) {
                                       break;
                                   }
                               }
                           }},
                       declaration);
        } catch (const Error& error) {
            throw Error(detail::at_line(line, error.what()));
        }
    }

    detail::topological_order(catalog, std::vector<bool>(catalog.assets_.size(), true));
    return catalog;
}
} // namespace assetforge
