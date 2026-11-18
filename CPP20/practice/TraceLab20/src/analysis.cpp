#include "tracelab/analysis.hpp"
#include <algorithm>
#include <bit>
#include <format>
#include <limits>
#include <ranges>

namespace tracelab {
void add(Count& value, Count increment) {
    if (increment > std::numeric_limits<Count>::max() - value) {
        throw std::overflow_error("statistic overflow");
    }
    value += increment;
}

void Query::validate() const {
    if (minimum > Level::error || top > max_top || flags > flag_mask ||
        (from && to && *from > *to) || components.size() > max_components ||
        !std::ranges::all_of(components, identifier) || keyword.size() > max_line ||
        !plain_text(keyword)) {
        throw std::invalid_argument("invalid query");
    }
}

bool Query::matches(const Event& e) const {
    return e.level >= minimum && (!from || e.key.time >= *from) && (!to || e.key.time < *to) &&
           (components.empty() || components.contains(e.component)) && (e.flags & flags) == flags &&
           e.message.find(keyword) != std::string::npos;
}

std::string Query::describe() const {
    std::string list;
    for (const auto& name : components) {
        if (!list.empty()) {
            list += ',';
        }
        list += name;
    }
    auto bound = [](std::optional<Time> t) { return t ? std::format("{:%FT%T}Z", *t) : "*"; };
    return std::format("level={} components={} flags={} from={} to={} top={} order={} keyword={}",
                       names.at(static_cast<unsigned>(minimum)), list.empty() ? "*" : list, flags,
                       bound(from), bound(to), top, latest ? "latest" : "earliest", keyword);
}

double Statistics::mean() const {
    return timed ? static_cast<double>(total_us) / static_cast<double>(timed) : 0.0;
}

namespace {
template <class Map, class K>
void group(Map& map, const K& key, Count n, std::size_t limit) {
    add(map[key], n);
    if (map.size() > limit) {
        throw std::runtime_error("group limit exceeded");
    }
}

void extend(Statistics& s, Time lo, Time hi) {
    if (!s.first || lo < *s.first) {
        s.first = lo;
    }
    if (!s.last || hi > *s.last) {
        s.last = hi;
    }
}

void duration(Statistics& s, Count n, Count total, Count lo, Count hi) {
    if (!n) {
        return;
    }
    if (!s.timed || lo < s.min_us) {
        s.min_us = lo;
    }
    s.max_us = std::max(s.max_us, hi);
    add(s.total_us, total);
    add(s.timed, n);
}
} // namespace

void Statistics::accept(const Event& e) {
    add(matched, 1);
    add(levels.at(static_cast<unsigned>(e.level)), 1);
    add(flag_bits, std::popcount(e.flags)); // [42]
    extend(*this, e.key.time, e.key.time);
    group(components, e.component, 1, max_components);
    group(minutes, std::chrono::floor<std::chrono::minutes>(e.key.time), 1, max_minutes);
    if (e.duration) {
        duration(*this, 1, *e.duration, *e.duration, *e.duration);
    }
}

void Statistics::merge(const Statistics& s) {
    add(processed, s.processed);
    add(invalid, s.invalid);
    add(matched, s.matched);
    add(flag_bits, s.flag_bits);
    duration(*this, s.timed, s.total_us, s.min_us, s.max_us);
    if (s.first) {
        extend(*this, *s.first, *s.last);
    }
    for (std::size_t i = 0; i < levels.size(); ++i) {
        add(levels[i], s.levels[i]);
    }
    for (const auto& [k, v] : s.components) {
        group(components, k, v, max_components);
    }
    for (const auto& [k, v] : s.minutes) {
        group(minutes, k, v, max_minutes);
    }
}

namespace {
template <class A>
concept Accumulator = requires(A a, const A& other, const Event& event) {
    { a.accept(event) } -> std::same_as<void>;
    { a.merge(other) } -> std::same_as<void>;
};
static_assert(Accumulator<Statistics> && !Accumulator<Event>);

template <class T, class Better, class Copy>
void retain(std::vector<T>& heap, const T& item, std::size_t k, Better better, Copy copy) {
    if (k == 0) {
        return;
    }
    if (heap.size() == k) {
        if (!better(item, heap.front())) {
            return;
        }
        std::ranges::pop_heap(heap, better);
        heap.pop_back();
    }
    heap.push_back(copy(item));
    std::ranges::push_heap(heap, better);
}

auto by_time(bool latest) {
    return
        [latest](const Event& a, const Event& b) { return latest ? a.key > b.key : a.key < b.key; };
}

bool slower(const Event& a, const Event& b) {
    return a.duration != b.duration ? a.duration > b.duration : a.key < b.key;
}

Event preview(const Event& event) {
    auto length = std::min<std::size_t>(512, event.message.size());
    bool truncated = length < event.message.size();
    if (truncated) {
        length -= 3;
        while ((static_cast<unsigned char>(event.message[length]) & 0xc0) == 0x80) {
            --length;
        }
    }
    return {event.key,
            event.level,
            event.component,
            event.pid,
            event.flags,
            event.duration,
            event.message.substr(0, length) + (truncated ? "..." : "")};
}

bool earlier(const Diagnostic& a, const Diagnostic& b) {
    return a.source < b.source;
}

template <Accumulator A, std::ranges::input_range R>
void accumulate(A& stats, R&& events, Summary& result, const Query& query) { // [31]
    for (const Event& event : events) {
        stats.accept(event);
        retain(result.timeline, event, query.top, by_time(query.latest), preview);
        if (event.duration) {
            retain(result.slowest, event, query.top, slower, preview);
        }
    }
}
} // namespace

Summary analyze_batch(std::span<const Record> rows, const Options& options, std::stop_token stop,
                      const std::vector<fs::path>& files) { // [37]
    Summary result;
    std::vector<Event> events;
    events.reserve(rows.size());
    for (const auto& row : rows) {
        if (stop.stop_requested()) {
            return {};
        }
        try {
            events.push_back(parse(row));
        } catch (const BadRecord& e) {
            if (options.strict) {
                throw BadRecord(std::format("{}:{}: {}",
                                            bytes(files.at(row.source.file).generic_u8string()),
                                            row.source.line, e.what()));
            }
            add(result.stats.invalid, 1);
            retain(result.diagnostics, Diagnostic{row.source, e.what()}, 100, earlier,
                   std::identity{});
        }
    }
    result.stats.processed = rows.size();
    // [33] The batch owns the events for the entire lifetime of this lazy view.
    auto matches = [&](const Event& e) { return options.query.matches(e); };
    auto matching = events | std::views::filter(matches);
    accumulate(result.stats, matching, result, options.query);
    return result;
}

void merge(Summary& target, const Summary& batch, const Query& query) {
    target.stats.merge(batch.stats);
    for (const auto& e : batch.timeline) {
        retain(target.timeline, e, query.top, by_time(query.latest), std::identity{});
    }
    for (const auto& e : batch.slowest) {
        retain(target.slowest, e, query.top, slower, std::identity{});
    }
    for (const auto& d : batch.diagnostics) {
        retain(target.diagnostics, d, 100, earlier, std::identity{});
    }
}

void finish(Summary& s, const Query& q) {
    std::ranges::sort(s.timeline, by_time(q.latest));
    std::ranges::sort(s.slowest, slower);
    std::ranges::sort(s.diagnostics, {}, &Diagnostic::source); // [32]
}
} // namespace tracelab
