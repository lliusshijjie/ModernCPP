#include "tracelab/analysis.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <format>
#include <istream>
#include <ostream>
#include <ranges>

namespace tracelab {
namespace {
using Buffer = std::vector<std::byte>;
constexpr std::size_t max_payload = 2 * 1024 * 1024;
constexpr auto little = std::endian::little;
static_assert(std::endian::native == little || std::endian::native == std::endian::big);
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);

void require(bool condition, std::string_view why) {
    if (!condition) {
        throw std::runtime_error(std::format("invalid TL20 snapshot: {}", why));
    }
}

class Writer {
public:
    template <class T>
    void scalar(T value) {
        auto raw = std::bit_cast<std::array<std::byte, sizeof(T)>>(value); // [42]
        if constexpr (std::endian::native != little) {
            std::ranges::reverse(raw);
        }
        data.insert(data.end(), raw.begin(), raw.end());
    }

    void text(std::string_view s) {
        require(s.size() <= max_payload, "string length");
        scalar(static_cast<std::uint32_t>(s.size()));
        auto raw = std::as_bytes(std::span(s));
        data.insert(data.end(), raw.begin(), raw.end());
    }

    Buffer data;
};

class Reader {
public:
    explicit Reader(std::span<const std::byte> data) : data_(data) {
    }

    template <class T>
    T scalar() {
        auto part = take(sizeof(T));
        std::array<std::byte, sizeof(T)> raw;
        std::ranges::copy(part, raw.begin());
        if constexpr (std::endian::native != little) {
            std::ranges::reverse(raw);
        }
        return std::bit_cast<T>(raw);
    }

    std::string text(std::size_t limit) {
        auto n = scalar<std::uint32_t>();
        require(n <= limit, "string length");
        auto raw = take(n);
        std::string value(reinterpret_cast<const char*>(raw.data()), raw.size());
        require(plain_text(value), "invalid UTF-8 or control delimiter");
        return value;
    }

    bool empty() const {
        return data_.empty();
    }

private:
    std::span<const std::byte> take(std::size_t n) {
        require(n <= data_.size(), "truncated payload");
        auto part = data_.first(n);
        data_ = data_.subspan(n);
        return part;
    }

    std::span<const std::byte> data_;
};

void check_time(Time t) {
    using namespace std::chrono;
    require(t >= sys_days(year(-1) / 1 / 1) && t < sys_days(year(10001) / 1 / 1),
            "timestamp out of range");
}

void check(const Statistics& s, double mean) {
    require(s.invalid <= s.processed && s.matched <= s.processed - s.invalid, "record counts");
    require(s.timed <= s.matched && mean == s.mean() && std::isfinite(mean), "duration count/mean");
    require(s.flag_bits / 3 < s.matched || (s.flag_bits / 3 == s.matched && s.flag_bits % 3 == 0),
            "flag count");
    if (s.timed == 1) {
        require(s.total_us == s.min_us && s.total_us == s.max_us, "single duration");
    } else if (s.timed >= 2) {
        require(s.min_us <= s.max_us && s.max_us <= s.total_us && s.min_us <= s.total_us - s.max_us,
                "duration extrema");
        // Remove one observation of each extremum. Division avoids overflowing n * max.
        const auto rest = s.total_us - s.min_us - s.max_us;
        const auto n = s.timed - 2;
        if (n == 0) {
            require(rest == 0, "two durations");
        } else {
            const auto quotient = rest / n;
            require(quotient >= s.min_us &&
                        (quotient < s.max_us || (quotient == s.max_us && rest % n == 0)),
                    "duration sum");
        }
    } else {
        require(s.total_us == 0 && s.min_us == 0 && s.max_us == 0, "empty durations");
    }

    auto sum = [](auto&& range) {
        Count total{};
        for (auto n : range) {
            add(total, n);
        }
        return total;
    };
    require(sum(s.levels) == s.matched && sum(s.components | std::views::values) == s.matched &&
                sum(s.minutes | std::views::values) == s.matched,
            "group totals");
    if (s.matched) {
        require(s.first && s.last && *s.first <= *s.last, "time range");
        check_time(*s.first);
        check_time(*s.last);
        require(s.minutes.begin()->first == std::chrono::floor<std::chrono::minutes>(*s.first) &&
                    s.minutes.rbegin()->first == std::chrono::floor<std::chrono::minutes>(*s.last),
                "minute bounds");
    } else {
        require(!s.first && !s.last, "empty time range");
    }
}

void write(std::ostream& out, const Buffer& data) {
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
    if (!out) {
        throw std::runtime_error("snapshot write failed");
    }
}

Buffer read(std::istream& in, std::size_t n) {
    Buffer data(n);
    if (n && !in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(n))) {
        throw std::runtime_error("truncated snapshot");
    }
    return data;
}

constexpr std::uint32_t magic = 0x30324c54; // TL20 in file order
} // namespace

void save_snapshot(const Report& r, std::ostream& out) {
    if (r.status != Status::complete && r.status != Status::warnings) {
        throw std::invalid_argument("snapshot requires a complete scan");
    }
    const auto& s = r.summary.stats;
    check(s, s.mean());
    Writer payload;
    payload.text(r.query);
    for (Count n : {s.processed, s.invalid, s.matched, s.flag_bits, s.timed, s.total_us, s.min_us,
                    s.max_us}) {
        payload.scalar(n);
    }
    payload.scalar(s.mean());
    for (auto n : s.levels) {
        payload.scalar(n);
    }
    if (s.matched) {
        payload.scalar<std::int64_t>(s.first->time_since_epoch().count());
        payload.scalar<std::int64_t>(s.last->time_since_epoch().count());
    }
    payload.scalar(static_cast<std::uint32_t>(s.components.size()));
    for (const auto& [name, n] : s.components) {
        payload.text(name);
        payload.scalar(n);
    }
    payload.scalar(static_cast<std::uint32_t>(s.minutes.size()));
    for (const auto& [minute, n] : s.minutes) {
        payload.scalar<std::int64_t>(minute.time_since_epoch().count());
        payload.scalar(n);
    }
    require(payload.data.size() <= max_payload, "payload length");
    Writer header;
    header.scalar(magic);
    header.scalar<std::uint16_t>(1);
    header.scalar<std::uint16_t>(0);
    header.scalar(static_cast<std::uint32_t>(payload.data.size()));
    write(out, header.data);
    write(out, payload.data);
}

Snapshot load_snapshot(std::istream& in) {
    auto header = read(in, 12);
    Reader h(header);
    require(h.scalar<std::uint32_t>() == magic, "magic");
    require(h.scalar<std::uint16_t>() == 1, "unsupported version");
    require(h.scalar<std::uint16_t>() == 0, "unknown flags");
    auto size = h.scalar<std::uint32_t>();
    require(size <= max_payload, "payload length");
    auto payload = read(in, size);
    require(in.peek() == std::char_traits<char>::eof() && !in.bad(), "trailing bytes or I/O error");
    Reader reader(payload);
    Snapshot result;
    result.query = reader.text(max_payload);
    auto& s = result.stats;
    for (auto p : {&s.processed, &s.invalid, &s.matched, &s.flag_bits, &s.timed, &s.total_us,
                   &s.min_us, &s.max_us}) {
        *p = reader.scalar<Count>();
    }
    auto mean = reader.scalar<double>();
    for (auto& n : s.levels) {
        n = reader.scalar<Count>();
    }
    if (s.matched) {
        s.first = Time(std::chrono::microseconds(reader.scalar<std::int64_t>()));
        s.last = Time(std::chrono::microseconds(reader.scalar<std::int64_t>()));
    }
    auto components = reader.scalar<std::uint32_t>();
    require(components <= max_components, "component limit");
    while (components--) {
        auto name = reader.text(128);
        auto n = reader.scalar<Count>();
        require(n && identifier(name) && s.components.emplace(std::move(name), n).second,
                "component row");
    }
    auto buckets = reader.scalar<std::uint32_t>();
    require(buckets <= max_minutes, "minute limit");
    while (buckets--) {
        auto key = std::chrono::sys_time<std::chrono::minutes>(
            std::chrono::minutes(reader.scalar<std::int64_t>()));
        auto n = reader.scalar<Count>();
        require(n && s.minutes.emplace(key, n).second, "minute row");
    }
    require(reader.empty(), "extra payload");
    check(s, mean);
    return result;
}
} // namespace tracelab
