#pragma once

#include <any>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace minikv {

// [26] 头文件中的共享常量；所有翻译单元引用同一个实体。
inline constexpr std::string_view kVersion = "MiniKV17/1.0";
inline constexpr std::size_t kMaxKey = 128;
inline constexpr std::size_t kMaxValue = 65'536;
inline constexpr std::size_t kMaxEntries = 10'000;

// 值和查询结果。
using Value = std::variant<std::string, std::int64_t, bool>; // [17] 闭合的业务类型集合

struct Entry {
    Value value;
    std::optional<std::int64_t> expires; // [16] nullopt 表示永久有效
};

struct Row {
    std::string key;
    Entry entry;
};

struct Error {
    std::string message;
};

struct Ok {
    std::string message;
};

template <class T>
using Result = std::variant<T, Error>;

using Status = Result<Ok>;
using Clock = std::function<std::int64_t()>;

struct Stats {
    std::uint64_t keys = 0;
    std::uint64_t value_bytes = 0;

    std::uint64_t writes = 0;
    std::uint64_t reads = 0;
    std::uint64_t hits = 0;

    std::uint64_t replayed = 0;
    std::uint64_t compactions = 0;
    std::uint64_t discarded_tail = 0;
    bool faulted = false;
};

// 日志写入的是绝对过期时间，不能在重启后重新计算相对 TTL。
struct Write {
    std::string key;
    Entry entry;
};

struct Erase {
    std::string key;
};

struct Rename {
    std::string from;
    std::string to;
};

using Mutation = std::variant<Write, Erase, Rename>;

// 命令行请求：保存相对 TTL，由 Store 在提交时计算绝对截止时间。
struct Put {
    std::string key;
    Value value;
    std::optional<std::int64_t> ttl;
};

struct Get {
    std::string key;
};

struct Increment {
    std::string key;
    std::int64_t delta;
};

struct Scan {
    std::string prefix;
};

struct ShowStats {};

struct Compact {};

struct Help {};

struct Quit {};

using Command =
    std::variant<Put, Get, Increment, Erase, Rename, Scan, ShowStats, Compact, Help, Quit>;

// [14] << 折叠：输出一个完整逻辑行，调用方负责线程同步。
template <class... Args>
void line(std::ostream& out, const Args&... args) {
    (out << ... << args) << '\n';
}

// [24] 全字段解析；不接受前后空白、溢出或 12tail 这样的部分成功。
template <class T>
[[nodiscard]] std::optional<T> number(std::string_view text) {
    static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>);

    if (text.empty()) {
        return std::nullopt;
    }

    T value{};
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }

    return value;
}

// [13] 不成立的分支不实例化：只有整数走 to_chars，bool 不误入整数分支。
template <class T>
std::string scalar_text(const T& value) {
    if constexpr (std::is_same_v<T, std::string>) {
        return value;
    } else if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    } else {
        char buffer[32];
        const auto [end, ec] = std::to_chars(buffer, buffer + sizeof buffer, value);
        if (ec != std::errc{}) {
            throw std::runtime_error("number formatting failed");
        }

        return std::string(buffer, end);
    }
}

[[nodiscard]] bool valid_key(std::string_view key);

[[nodiscard]] bool expired(const Entry& entry, std::int64_t now);

[[nodiscard]] std::uint64_t value_size(const Value& value);

std::int64_t wall_seconds();

std::string display(const Value& value);

std::string quote(std::string_view text);

// [19] variant 区分解析失败；optional 只表达字段缺失，不吞掉错误。
[[nodiscard]] Result<Command> parse_command(std::string_view text);

} // namespace minikv
