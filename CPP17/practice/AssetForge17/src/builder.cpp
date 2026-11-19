#include "detail.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <tuple>
#include <type_traits>

namespace assetforge {
namespace {
class StagingFile {
public:
    explicit StagingFile(const fs::path& parent) {
        static std::atomic<unsigned> serial{};
        const auto time = std::chrono::steady_clock::now().time_since_epoch().count();

        for (unsigned attempt = 0; attempt < 32; ++attempt) {
            directory_ =
                parent / (".assetforge-" + std::to_string(time) + "-" + std::to_string(serial++));
            file_ = directory_ / "staged.afp";
            if (fs::create_directory(directory_)) {
                return;
            }
        }
        throw Error("could not reserve a staging directory");
    }

    ~StagingFile() {
        std::error_code ignored;
        fs::remove(file_, ignored);
        fs::remove(directory_, ignored);
    }

    StagingFile(const StagingFile&) = delete;
    StagingFile& operator=(const StagingFile&) = delete;

    const fs::path& path() const noexcept {
        return file_;
    }

private:
    fs::path directory_, file_;
};

struct PreparedAsset {
    Bytes bytes;
    Size source_size;
    Size replacements;
};

fs::path resolve_source(const fs::path& root, const fs::path& relative) {
    auto file = fs::canonical(root / relative);
    auto within = file.lexically_relative(root);
    if (within.empty() || within.is_absolute() || *within.begin() == "..") {
        throw Error("resource path escapes manifest directory");
    }
    return file;
}

template <class Mode>
PreparedAsset prepare(const fs::path& file, Mode, std::optional<Size> limit, Size budget) {
    auto bytes = detail::read_file(file, std::min(max_file, budget));
    const Size source_size = bytes.size();
    Size replacements{};

    // [13] Only the text policy performs the normalization pass.
    if constexpr (std::is_same_v<Mode, TextLfSource>) {
        std::size_t output{};
        for (std::size_t input = 0; input < bytes.size(); ++input) {
            auto byte = bytes[input];
            if (byte == std::byte{0}) {
                throw Error("NUL in text-lf resource");
            }
            if (byte == std::byte{'\r'}) {
                if (input + 1 == bytes.size() || bytes[input + 1] != std::byte{'\n'}) {
                    throw Error("bare CR in text-lf resource");
                }
                byte = bytes[++input];
                ++replacements;
            }
            bytes[output++] = byte;
        }
        bytes.resize(output);
    }
    if (bytes.size() > limit.value_or(max_file)) {
        throw Error("processed byte limit exceeded");
    }
    return {std::move(bytes), source_size, replacements};
}

PreparedAsset prepare_asset(const fs::path& root, const AssetSpec& asset, Size budget) {
    try {
        auto parameters = std::tuple{resolve_source(root, asset.path), asset.limit, budget}; // [15]
        return std::visit(
            [&](auto mode) {
                // [29] The tuple owns the path and limits supplied to the typed preparer.
                return std::apply(
                    [mode](const auto& file, auto limit, Size max_input) {
                        return prepare(file, mode, limit, max_input);
                    },
                    parameters);
            },
            asset.mode);
    } catch (const Error& error) {
        throw Error(asset.id + " (" + asset.path.u8string() + "): " + error.what());
    } catch (const fs::filesystem_error& error) {
        throw Error(asset.id + " (" + asset.path.u8string() + "): " + error.what());
    }
}

void write_plan(const BuildPlan& plan, const fs::path& file, Policy policy, BuildSummary& summary) {
    std::vector<PreparedAsset> assets;
    std::vector<Entry> entries;
    std::vector<detail::Block> blocks;
    Size source_total{};
    assets.reserve(plan.assets().size());
    entries.reserve(plan.assets().size());
    blocks.reserve(plan.assets().size());

    for (const auto& item : plan.assets()) {
        auto asset = prepare_asset(plan.directory(), item.spec, max_content - source_total);
        const auto length = static_cast<Size>(asset.bytes.size());
        source_total += asset.source_size;
        if (length > max_content - summary.content_size) {
            throw Error("content size limit exceeded");
        }

        entries.push_back({item.spec.id, summary.content_size, length, 0, item.dependencies});
        summary.metrics.push_back({length, asset.replacements});
        summary.content_size += length;
        assets.push_back(std::move(asset));
    }

    for (const auto& asset : assets) {
        blocks.push_back({asset.bytes.data(), asset.bytes.size()});
    }
    auto crc = detail::checksums(blocks, policy);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        entries[i].crc = crc[i];
    }
    detail::write_bundle(file, entries, plan.aliases(), blocks, summary.content_size);
}

void require_absent(const fs::path& path) {
    if (fs::exists(fs::symlink_status(path))) {
        throw Error("output already exists: " + path.u8string());
    }
}
} // namespace

BuildResult build(const BuildPlan& plan, const fs::path& output, Policy policy) {
    try {
        if (output.empty() || !output.has_filename()) {
            throw Error("expected output file path");
        }
        if (plan.assets().empty()) {
            throw Error("empty build plan");
        }
        const auto path = fs::absolute(output);
        require_absent(path);

        fs::create_directories(path.parent_path());
        StagingFile stage(path.parent_path());
        BuildSummary summary;
        summary.alias_count = plan.aliases().size();
        write_plan(plan, stage.path(), policy, summary);

        {
            auto reader = BundleReader::open(stage.path());
            auto failed = reader.verify(policy);
            if (!failed.empty()) {
                throw Error("staged verification failed: " + failed.front());
            }
        }

        require_absent(path);
        fs::rename(stage.path(), path);
        return summary;
    } catch (const Error& error) {
        return BuildError{error.what()};
    } catch (const fs::filesystem_error& error) {
        return BuildError{error.what()};
    }
}
} // namespace assetforge
