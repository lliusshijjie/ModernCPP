#include "tracelab/analysis.hpp"
#include <algorithm>
#include <format>
#include <ranges>

namespace tracelab {
using namespace std::chrono;

Zone::Zone(std::string_view name) {
    if (name == "UTC" || name == "Z") {
        return;
    }
    if (name.starts_with('+') || name.starts_with('-')) {
        fixed_ = offset(name);
    } else {
        try {
            named_ = locate_zone(name);
        } catch (const std::exception& e) {
            throw std::invalid_argument(std::format("time zone '{}': {}", name, e.what()));
        }
    }
}

std::string Zone::display(Time t) const {
    if (named_) {
        return std::format("{:%FT%T%Ez}", zoned_time{named_, t});
    }
    if (fixed_ == minutes{}) {
        return std::format("{:%FT%T}Z", t);
    }
    auto n = std::abs(fixed_.count());
    return std::format("{:%FT%T}{}{:02}:{:02}", t + fixed_, fixed_.count() < 0 ? '-' : '+', n / 60,
                       n % 60);
}

std::string render_statistics(const Statistics& s, std::string_view query, const Zone& zone) {
    std::string out = std::format("query {}\nprocessed={} invalid={} matched={} flag_bits={}\n",
                                  query, s.processed, s.invalid, s.matched, s.flag_bits);
    out += std::format("timed={} total_us={} min_us={} max_us={} mean_us={}\n", s.timed, s.total_us,
                       s.timed ? std::to_string(s.min_us) : "-",
                       s.timed ? std::to_string(s.max_us) : "-",
                       s.timed ? std::format("{:.2f}", s.mean()) : "-");
    if (s.first) {
        out += std::format("range {} .. {}\n", zone.display(*s.first), zone.display(*s.last));
    }
    std::vector<Level> populated{Level::trace, Level::debug, Level::info, Level::warn,
                                 Level::error};
    std::erase_if(populated,
                  [&](Level l) { return s.levels.at(static_cast<unsigned>(l)) == 0; }); // [43]
    for (auto l : populated) {
        auto i = static_cast<unsigned>(l);
        out += std::format("level {:5} {}\n", names[i], s.levels[i]); // [36]
    }
    for (const auto& [component, n] : s.components) {
        out += std::format("component {} {}\n", component, n);
    }
    for (const auto& [minute, n] : s.minutes) {
        out += std::format("minute {} {}\n", zone.display(minute), n);
    }
    return out;
}

std::string render(const Report& r, const Zone& zone) {
    std::string out =
        r.status == Status::cancelled ? "PARTIAL (committed batches)\n" : "COMPLETE\n";
    out += render_statistics(r.summary.stats, r.query, zone);
    auto location = [&](Position p) {
        return std::format("{}:{}", bytes(r.files.at(p.file).generic_u8string()), p.line);
    };
    auto rows = [&](std::string_view title, const std::vector<Event>& events) {
        out += std::format("[{}]\n", title);
        for (const auto& e : events) {
            out += std::format("{} {} {} pid={} flags={} us={} {} {}\n", zone.display(e.key.time),
                               names.at(static_cast<unsigned>(e.level)), e.component, e.pid,
                               e.flags, e.duration ? std::to_string(*e.duration) : "-",
                               location(e.key.source), e.message);
        }
    };
    rows("timeline", r.summary.timeline);
    rows("slowest", r.summary.slowest);
    for (const auto& d : r.summary.diagnostics) {
        out += std::format("invalid {} {}\n", location(d.source), d.message);
    }
    return out;
}
} // namespace tracelab
