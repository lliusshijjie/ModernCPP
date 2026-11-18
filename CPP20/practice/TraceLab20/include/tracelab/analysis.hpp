#pragma once
#include "input.hpp"
#include <functional>
#include <span>

namespace tracelab {
struct Options {
    Query query;
    unsigned workers{2};
    bool strict{};
};

struct Progress {
    Count read{}, processed{}, matched{};
    bool done{};
    Count session{};
};

using Observer = std::function<void(Progress)>;

void add(Count& value, Count increment);
Summary analyze_batch(std::span<const Record>, const Options&, std::stop_token,
                      const std::vector<fs::path>&);
void merge(Summary&, const Summary&, const Query&);
void finish(Summary&, const Query&);
// The session owns its query while worker threads read it.
Report analyze(std::vector<fs::path>, Options, std::stop_token = {}, Observer = {});
Report analyze_serial(std::vector<fs::path>, const Options&);

class Zone {
public:
    explicit Zone(std::string_view name);
    std::string display(Time) const;

private:
    std::chrono::minutes fixed_{};
    const std::chrono::time_zone* named_{};
};

std::string render_statistics(const Statistics&, std::string_view query, const Zone&);
std::string render(const Report&, const Zone&);

struct Snapshot {
    Statistics stats;
    std::string query;
};

void save_snapshot(const Report&, std::ostream&);
Snapshot load_snapshot(std::istream&);
} // namespace tracelab
