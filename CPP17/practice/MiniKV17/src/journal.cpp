#include "minikv/journal.hpp"

#include <array>
#include <limits>

namespace minikv {

namespace {

using Bytes = std::vector<std::byte>;        // [30] 原始字节不是字符，也不隐式参与整数计算
constexpr std::uint32_t kMagic = 0x3731564b; // little endian: KV17
constexpr std::size_t kHeader = 16;
constexpr std::size_t kMaxPayload = kMaxValue + 2 * kMaxKey + 64;

void integer(Bytes& out, std::uint64_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        out.push_back(std::byte((value >> (i * 8)) & 255));
    }
}

void text(Bytes& out, std::string_view value) {
    integer(out, value.size(), 4);
    for (unsigned char c : value) {
        out.push_back(std::byte{c});
    }
}

class Reader {
public:
    explicit Reader(const Bytes& bytes) : bytes_(bytes) {
    }

    std::uint64_t integer(unsigned count) {
        if (count > bytes_.size() - pos_) {
            throw std::runtime_error("truncated field");
        }

        std::uint64_t value = 0;
        for (unsigned i = 0; i < count; ++i) {
            value |= std::uint64_t(std::to_integer<unsigned char>(bytes_[pos_++])) << (i * 8);
        }

        return value;
    }

    std::string text(std::size_t limit) {
        const auto size = integer(4);
        if (size > limit || size > bytes_.size() - pos_) {
            throw std::runtime_error("bad field length");
        }

        std::string result;
        result.reserve(static_cast<std::size_t>(size));
        for (std::uint64_t i = 0; i < size; ++i) {
            result += static_cast<char>(std::to_integer<unsigned char>(bytes_[pos_++]));
        }

        return result;
    }

    bool done() const {
        return pos_ == bytes_.size();
    }

private:
    const Bytes& bytes_;
    std::size_t pos_ = 0;
};

std::uint32_t checksum(const Bytes& bytes) {
    std::uint32_t hash = 2166136261u;
    for (auto byte : bytes) {
        hash = (hash ^ std::to_integer<unsigned char>(byte)) * 16777619u;
    }

    return hash; // FNV-1a：意外损坏检测，不是密码学认证
}

Bytes encode(const Mutation& mutation) {
    Bytes bytes;
    std::visit(
        [&](const auto& record) {
            using T = std::decay_t<decltype(record)>;

            if constexpr (std::is_same_v<T, Write>) {
                integer(bytes, 1, 1);
                text(bytes, record.key);
                integer(bytes, static_cast<std::uint64_t>(record.entry.expires.value_or(0)), 8);
                integer(bytes, record.entry.value.index(), 1);

                std::visit([&](const auto& item) { text(bytes, scalar_text(item)); },
                           record.entry.value);
            } else if constexpr (std::is_same_v<T, Erase>) {
                integer(bytes, 2, 1);
                text(bytes, record.key);
            } else {
                integer(bytes, 3, 1);
                text(bytes, record.from);
                text(bytes, record.to);
            }
        },
        mutation);

    return bytes;
}

Mutation decode(const Bytes& bytes) {
    Reader reader(bytes);
    const auto op = reader.integer(1);
    auto key = reader.text(kMaxKey);
    if (!valid_key(key)) {
        throw std::runtime_error("bad journal key");
    }

    Mutation result = Erase{key};
    if (op == 1) {
        const auto expiry = reader.integer(8);
        if (expiry > std::uint64_t(std::numeric_limits<std::int64_t>::max())) {
            throw std::runtime_error("bad journal expiry");
        }

        const auto tag = reader.integer(1);
        auto value_text = reader.text(kMaxValue);
        Value value = value_text;
        if (tag == 1) {
            auto value_number = number<std::int64_t>(value_text);
            if (!value_number) {
                throw std::runtime_error("bad journal integer");
            }
            value = *value_number;
        } else if (tag == 2 && (value_text == "true" || value_text == "false")) {
            value = value_text == "true";
        } else if (tag != 0) {
            throw std::runtime_error("bad journal value type");
        }

        std::optional<std::int64_t> expires;
        if (expiry) {
            expires = static_cast<std::int64_t>(expiry);
        }

        result = Write{std::move(key), Entry{std::move(value), expires}};
    } else if (op == 3) {
        auto to = reader.text(kMaxKey); // [28] 分步消费字节，绝不假定函数实参从左到右求值
        if (!valid_key(to)) {
            throw std::runtime_error("bad rename target");
        }
        result = Rename{std::move(key), std::move(to)};
    } else if (op != 2) {
        throw std::runtime_error("unknown journal operation");
    }

    if (!reader.done()) {
        throw std::runtime_error("trailing journal fields");
    }

    return result;
}

void write_frame(std::ostream& out, const Mutation& mutation) {
    const auto payload = encode(mutation); // 所有分配都发生在第一次文件写入之前
    Bytes frame;
    integer(frame, kMagic, 4);
    integer(frame, payload.size(), 4);
    integer(frame, checksum(payload), 4);
    integer(frame, checksum(frame), 4); // 先验证长度字段自身，再按长度读 payload
    frame.insert(frame.end(), payload.begin(), payload.end());

    out.write(reinterpret_cast<const char*>(frame.data()),
              static_cast<std::streamsize>(frame.size()));
}

} // namespace

Journal::Journal(fs::path directory)
    : active_(directory / "store.wal"), backup_(directory / "store.backup"),
      next_(directory / "store.next") {
    fs::create_directories(directory);
    if (!fs::exists(active_) && fs::exists(backup_)) {
        fs::rename(backup_, active_);
    }
}

void Journal::open_append() {
    output_.clear();
    output_.open(active_, std::ios::binary | std::ios::app);
    if (!output_) {
        throw std::runtime_error("cannot open journal for append");
    }
}

Recovery Journal::recover() {
    Recovery result;
    std::uintmax_t valid = 0;

    if (fs::exists(active_)) {
        std::ifstream input(active_, std::ios::binary);
        if (!input) {
            throw std::runtime_error("cannot open journal for recovery");
        }

        while (true) {
            Bytes header(kHeader);
            input.read(reinterpret_cast<char*>(header.data()), kHeader);
            const auto count = input.gcount();
            if (input.bad()) {
                throw std::runtime_error("journal read failed");
            }
            if (count == 0 && input.eof()) {
                break;
            }
            if (count != static_cast<std::streamsize>(kHeader)) {
                result.discarded = fs::file_size(active_) - valid;
                break;
            }

            // 先校验帧头，再信任长度字段并分配载荷缓冲区。
            Reader reader(header);
            const Bytes protected_header(header.begin(), header.begin() + 12);
            if (reader.integer(4) != kMagic) {
                throw std::runtime_error("journal magic mismatch");
            }
            const auto size = reader.integer(4);
            const auto expected = reader.integer(4);
            if (reader.integer(4) != checksum(protected_header)) {
                throw std::runtime_error("journal header checksum mismatch");
            }
            if (size == 0 || size > kMaxPayload) {
                throw std::runtime_error("journal frame too large/empty");
            }

            Bytes payload(static_cast<std::size_t>(size));
            input.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(size));
            if (input.bad()) {
                throw std::runtime_error("journal payload read failed");
            }
            if (input.gcount() != static_cast<std::streamsize>(size)) {
                result.discarded = fs::file_size(active_) - valid;
                break;
            }
            if (checksum(payload) != expected) {
                throw std::runtime_error("journal checksum mismatch");
            }

            result.records.push_back(decode(payload));
            valid += kHeader + size;
        }

        input.close(); // Windows 上关闭读句柄后再 resize/rename
        if (result.discarded) {
            fs::resize_file(active_, valid);
        }
    }

    open_append();
    std::error_code ignored;
    fs::remove(backup_, ignored);
    fs::remove(next_, ignored); // 未完成切换的临时文件从未成为提交依据

    return result;
}

Status Journal::append(const Mutation& mutation) {
    if (faulted_) {
        return Error{"journal faulted; close and reopen store"};
    }

    write_frame(output_, mutation);
    output_.flush();
    if (!output_) {
        faulted_ = true; // 写入失败时提交结果不确定；禁止继续追加掩盖损坏
        return Error{"journal write failed; outcome uncertain, reopen required"};
    }

    return Ok{"committed"};
}

Status Journal::rewrite(const std::vector<Row>& rows) {
    if (faulted_) {
        return Error{"journal faulted; close and reopen store"};
    }

    try {
        {
            std::ofstream next(next_, std::ios::binary | std::ios::trunc);
            if (!next) {
                return Error{"cannot create compacted journal"};
            }

            for (const auto& row : rows) {
                write_frame(next, Write{row.key, row.entry});
            }

            next.flush();
            if (!next) {
                return Error{"compaction write failed"};
            }

            next.close();
            if (!next) {
                return Error{"compaction close failed"};
            }
        }

        // 临时文件完整写好后，才关闭旧日志并切换文件名。
        output_.close();
        if (!output_) {
            throw std::runtime_error("journal close failed");
        }

        fs::remove(backup_);
        fs::rename(active_, backup_);
        fs::rename(next_, active_);

        open_append();
        std::error_code ignored;
        fs::remove(backup_, ignored);

        return Ok{"compacted"};
    } catch (const std::exception& error) {
        faulted_ = true; // 保留现场；重开时使用 active 或恢复 backup
        return Error{std::string("compaction interrupted; reopen required: ") + error.what()};
    }
}

} // namespace minikv
