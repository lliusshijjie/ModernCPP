#include "detail.hpp"
#include <cassert>

namespace assetforge {
namespace {
void require(bool condition, std::string_view why) {
    if (!condition) {
        throw Error("invalid AFP: " + std::string(why));
    }
}

class Encoder {
public:
    template <class T>
    Encoder& number(T value) {
        // [30] Integer encoding is explicit and independent of object layout.
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            bytes.push_back(static_cast<std::byte>(value & 0xffu));
            value >>= 8;
        }
        return *this;
    }

    void name(std::string_view name) {
        number(static_cast<std::uint16_t>(name.size()));
        for (auto c : name) {
            bytes.push_back(static_cast<std::byte>(c));
        }
    }

    Bytes bytes;
};

class Decoder {
public:
    explicit Decoder(const Bytes& bytes) : bytes_(bytes) {
    }

    template <class T>
    T number() {
        require(sizeof(T) <= bytes_.size() - cursor_, "truncated field");
        Size value{};
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            value |= std::to_integer<Size>(bytes_[cursor_++]) << (8 * i);
        }
        return static_cast<T>(value);
    }

    std::string name() {
        auto length = number<std::uint16_t>();
        require(length > 0 && length <= 64 && length <= bytes_.size() - cursor_, "name length");
        std::string value(reinterpret_cast<const char*>(bytes_.data() + cursor_), length);
        cursor_ += length;
        require(valid_name(value), "invalid name");
        return value;
    }

    bool done() const {
        return cursor_ == bytes_.size();
    }

private:
    const Bytes& bytes_;
    std::size_t cursor_{};
};

detail::Block view(const Bytes& bytes) {
    return {bytes.data(), bytes.size()};
}

void write_block(std::ostream& stream, detail::Block block) {
    if (block.size) {
        stream.write(reinterpret_cast<const char*>(block.data),
                     static_cast<std::streamsize>(block.size));
    }
}
} // namespace

void detail::write_bundle(const fs::path& path, const std::vector<Entry>& entries,
                          const Names& aliases, const std::vector<Block>& blocks,
                          Size content_size) {
    Encoder index;
    for (const auto& entry : entries) {
        index.name(entry.id);
        index.number(entry.offset)
            .number(entry.size)
            .number(entry.crc)
            .number(static_cast<std::uint32_t>(entry.dependencies.size())); // [28]
        for (auto dependency : entry.dependencies) {
            index.number(dependency);
        }
    }
    for (const auto& [name, target] : aliases) {
        index.name(name);
        index.number(target);
    }
    if (index.bytes.size() > max_index) {
        throw Error("bundle index limit exceeded");
    }

    Encoder header;
    for (auto c : magic) {
        header.bytes.push_back(static_cast<std::byte>(c));
    }
    header.number(version)
        .number(std::uint16_t{0})
        .number(static_cast<std::uint32_t>(entries.size()))
        .number(static_cast<std::uint32_t>(aliases.size()))
        .number(static_cast<std::uint32_t>(index.bytes.size()))
        .number(crc32(view(index.bytes)))
        .number(content_size);
    [[maybe_unused]] const auto encoded_header_size = header.bytes.size();
    assert(encoded_header_size == header_size);

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw Error("cannot create staged bundle: " + path.u8string());
    }
    write_block(out, view(header.bytes));
    write_block(out, view(index.bytes));
    for (auto block : blocks) {
        write_block(out, block);
    }
    out.close();
    if (!out) {
        throw Error("staged bundle write failed: " + path.u8string());
    }
}

BundleReader BundleReader::open(const fs::path& file) {
    return BundleReader(
        file); // [27] The result is constructed directly, even with both moves deleted.
}

BundleReader::BundleReader(const fs::path& file) {
    if (!fs::is_regular_file(file)) {
        throw Error("expected regular bundle file: " + file.u8string());
    }
    stream_.open(file, std::ios::binary | std::ios::ate);
    if (!stream_) {
        throw Error("cannot open bundle: " + file.u8string());
    }

    const auto file_size = static_cast<std::streamoff>(stream_.tellg());
    require(file_size >= static_cast<std::streamoff>(header_size), "truncated header");
    auto header = read_at(0, header_size);
    Decoder fields(header);

    // Each read advances its cursor before the next begins.
    for (auto c : magic) {
        require(fields.number<unsigned char>() == static_cast<unsigned char>(c), "magic");
    }
    require(fields.number<std::uint16_t>() == version, "unsupported version");
    require(fields.number<std::uint16_t>() == 0, "reserved flags");
    const auto entry_count = fields.number<std::uint32_t>();
    const auto alias_count = fields.number<std::uint32_t>();
    const auto index_size = fields.number<std::uint32_t>();
    const auto index_crc = fields.number<std::uint32_t>();
    content_size_ = fields.number<Size>();

    require(entry_count > 0 && entry_count <= max_assets && alias_count <= max_aliases,
            "entry/alias count");
    require(index_size <= max_index && content_size_ <= max_content,
            "index or content exceeds limit");
    content_base_ = header_size + index_size;
    require(static_cast<Size>(file_size) == content_base_ + content_size_, "file size mismatch");
    auto index = read_at(header_size, index_size);
    require(detail::crc32(view(index)) == index_crc, "index CRC");
    Decoder decoder(index);

    entries_.reserve(entry_count);
    Size next_offset{};
    std::size_t remaining_edges = max_edges;
    for (Index i = 0; i < entry_count; ++i) {
        auto name = decoder.name();
        require(ids_.try_emplace(name, i).second, "duplicate asset ID");
        const auto offset =
            decoder.number<Size>(); // [28] Cursor reads deliberately use separate statements.
        const auto length = decoder.number<Size>();
        const auto crc = decoder.number<std::uint32_t>();
        const auto dep_count = decoder.number<std::uint32_t>();
        require(offset == next_offset && length <= max_file &&
                    length <= content_size_ - next_offset,
                "entry bounds: " + name);
        require(dep_count <= i && dep_count <= remaining_edges, "dependency count");
        remaining_edges -= dep_count;

        Entry entry{std::move(name), offset, length, crc, {}};
        for (Index j = 0; j < dep_count; ++j) {
            const auto dependency = decoder.number<Index>();
            require(dependency < i &&
                        (entry.dependencies.empty() || entry.dependencies.back() < dependency),
                    "dependency order");
            entry.dependencies.push_back(dependency);
        }
        entries_.push_back(std::move(entry));
        next_offset += length;
    }

    for (Index i = 0; i < alias_count; ++i) {
        auto name = decoder.name();
        auto target = decoder.number<Index>();
        require(target < entry_count && ids_.find(name) == ids_.end(), "alias target or name");
        require(aliases_.empty() || aliases_.rbegin()->first < name, "alias ordering or duplicate");
        aliases_.try_emplace(std::move(name), target);
    }
    require(decoder.done() && next_offset == content_size_, "unused index or payload bytes");
}

Bytes BundleReader::read_at(Size offset, Size count) {
    stream_.clear();
    stream_.seekg(static_cast<std::streamoff>(offset));
    if (!stream_) {
        throw Error("bundle seek failed");
    }
    Bytes bytes(static_cast<std::size_t>(count));
    detail::read_exact(stream_, bytes);
    return bytes;
}

std::optional<Index> BundleReader::find(std::string_view name) const {
    if (auto it = ids_.find(name); it != ids_.end()) {
        return it->second;
    }
    if (auto it = aliases_.find(name); it != aliases_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<Bytes> BundleReader::read(std::string_view name) {
    auto id = find(name);
    if (!id) {
        return std::nullopt;
    }
    const auto& entry = entries_[*id];
    auto bytes = read_at(content_base_ + entry.offset, entry.size);
    require(detail::crc32(view(bytes)) == entry.crc, "payload CRC: " + entry.id);
    return bytes;
}

std::vector<std::string> BundleReader::verify(Policy policy) {
    auto bytes = read_at(content_base_, content_size_);
    std::vector<detail::Block> blocks;
    blocks.reserve(entries_.size());
    for (const auto& entry : entries_) {
        const auto size = static_cast<std::size_t>(entry.size);
        blocks.push_back({size ? bytes.data() + entry.offset : nullptr, size});
    }

    auto actual = detail::checksums(blocks, policy);
    std::vector<std::string> bad;
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (actual[i] != entries_[i].crc) {
            bad.push_back(entries_[i].id);
        }
    }
    return bad;
}
} // namespace assetforge
