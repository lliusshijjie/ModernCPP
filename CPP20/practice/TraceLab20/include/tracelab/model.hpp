#pragma once
#include "status.hpp"
#include <array>
#include <chrono>
#include <compare>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace tracelab {
namespace fs = std::filesystem;
using Count = std::uint64_t;
using Time = std::chrono::sys_time<std::chrono::microseconds>;
enum class Level : unsigned { trace, debug, info, warn, error };
inline constexpr std::array<std::string_view, 5> names{"TRACE", "DEBUG", "INFO", "WARN", "ERROR"};
inline constexpr unsigned flag_mask = 7;
inline constexpr std::size_t max_line = 65536, max_top = 1000, max_components = 1024,
                             max_minutes = 10000;

struct Position {
    std::size_t file{};
    Count line{};
    auto operator<=>(const Position&) const = default;
};

struct Key { // [35] Total order also breaks timestamp ties across files and workers.
    Time time;
    Position source;
    auto operator<=>(const Key&) const = default;
};

struct Record {
    Position source;
    std::string bytes;
};

struct Event {
    Key key;
    Level level{};
    std::string component;
    std::uint32_t pid{};
    unsigned flags{};
    std::optional<Count> duration;
    std::string message;
};

struct Query {
    Level minimum{};
    std::set<std::string> components;
    std::string keyword;
    std::optional<Time> from, to;
    unsigned flags{};
    std::size_t top{20};
    bool latest{};
    void validate() const;
    bool matches(const Event& event) const;
    std::string describe() const;
};

struct Statistics {
    Count processed{}, invalid{}, matched{}, flag_bits{};
    Count timed{}, total_us{}, min_us{}, max_us{};
    std::array<Count, 5> levels{};
    std::map<std::string, Count> components;
    std::map<std::chrono::sys_time<std::chrono::minutes>, Count> minutes;
    std::optional<Time> first, last;

    double mean() const;
    void accept(const Event&);
    void merge(const Statistics&);
    bool operator==(const Statistics&) const = default;
};

struct Diagnostic {
    Position source;
    std::string message;
};

struct Summary {
    Statistics stats;
    std::vector<Event> timeline, slowest;
    std::vector<Diagnostic> diagnostics;
};

struct Report {
    Status status{Status::complete};
    std::string query;
    std::vector<fs::path> files;
    Summary summary;
};
} // namespace tracelab
