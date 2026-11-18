#include "tracelab/analysis.hpp"
#include "tracelab/entry.hpp"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <format>
#include <iostream>
#include <mutex>
#include <ranges>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace tracelab {
namespace {
using namespace std::chrono;
constexpr std::string_view help = R"(TraceLab20
  tracelab analyze --input FILE|DIR [--input ...] [options]
  tracelab show-snapshot FILE [--timezone ZONE]
  --extension .log     directory file suffix (non-recursive)
  --min-level LEVEL    TRACE, DEBUG, INFO, WARN, ERROR
  --component A,B      match any named component
  --keyword TEXT       exact UTF-8 substring
  --flags MASK         all requested bits must be present (0..7)
  --from TIME --to TIME   UTC or offset timestamps, half-open interval
  --top N [--latest]   up to N timeline rows and N slowest rows
  --workers N          1..8 (default 2)
  --strict             fail on malformed input
  --progress           write counters to stderr
  --timeout Nms|Ns     request cancellation after the interval
  --timezone ZONE      UTC, +/-HH:MM or IANA zone name
  --report FILE        save UTF-8 text
  --snapshot FILE      save aggregate statistics for a complete scan
)";

struct Command {
    Options options;
    std::vector<fs::path> inputs;
    std::string extension{".log"}, timezone{"UTC"};
    std::optional<fs::path> report, snapshot;
    std::optional<milliseconds> timeout;
    bool progress{};
};

fs::path path_from(std::string_view s) {
    if (s.empty() || !valid_utf8(s)) {
        throw std::invalid_argument("expected nonempty UTF-8 path");
    }
    std::u8string u8;
    for (unsigned char c : s) {
        u8.push_back(static_cast<char8_t>(c));
    }
    return fs::path(u8);
}

milliseconds interval(std::string_view s) {
    unsigned factor = 1000;
    if (s.ends_with("ms")) {
        s.remove_suffix(2);
        factor = 1;
    } else if (s.ends_with('s')) {
        s.remove_suffix(1);
    } else {
        throw std::invalid_argument("timeout requires ms or s");
    }
    auto n = number<Count>(s);
    if (n > 86400000 / factor) {
        throw std::invalid_argument("timeout exceeds 24h");
    }
    return milliseconds(n * factor);
}

Command parse_command(std::span<const std::string> args) {
    Command cmd;
    auto& q = cmd.options.query;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& key = args[i];
        if (key == "--strict") {
            cmd.options.strict = true;
            continue;
        }
        if (key == "--latest") {
            q.latest = true;
            continue;
        }
        if (key == "--progress") {
            cmd.progress = true;
            continue;
        }
        if (++i == args.size()) {
            throw std::invalid_argument("missing argument to " + key);
        }
        const auto& v = args[i];
        if (key == "--input") {
            cmd.inputs.push_back(path_from(v));
        } else if (key == "--report") {
            cmd.report = path_from(v);
        } else if (key == "--snapshot") {
            cmd.snapshot = path_from(v);
        } else if (key == "--timezone") {
            cmd.timezone = v;
        } else if (key == "--workers") {
            cmd.options.workers = number<unsigned>(v);
        } else if (key == "--extension") {
            if (!v.starts_with('.') || v.size() < 2 || !identifier(v)) {
                throw std::invalid_argument("invalid extension");
            }
            cmd.extension = v;
        } else if (key == "--min-level") {
            auto name = v;
            for (char& c : name) {
                if (c >= 'a' && c <= 'z') {
                    c -= 'a' - 'A';
                }
            }
            q.minimum = parse_level(name);
        } else if (key == "--component") {
            if (v.empty()) {
                throw std::invalid_argument("empty component list");
            }
            for (auto part : v | std::views::split(',')) { // [34]
                std::string name(part.begin(), part.end());
                if (!identifier(name)) {
                    throw std::invalid_argument("invalid component list");
                }
                q.components.insert(std::move(name));
            }
        } else if (key == "--from") {
            q.from = timestamp(v);
        } else if (key == "--to") {
            q.to = timestamp(v);
        } else if (key == "--flags") {
            q.flags = number<unsigned>(v);
        } else if (key == "--keyword") {
            q.keyword = v;
        } else if (key == "--top") {
            q.top = number<std::size_t>(v);
        } else if (key == "--timeout") {
            cmd.timeout = interval(v);
        } else {
            throw std::invalid_argument("unknown option: " + key);
        }
    }
    return cmd;
}

bool same_file(const fs::path& a, const fs::path& b) {
    if (fs::exists(a) && fs::exists(b)) {
        return fs::equivalent(a, b);
    }
    auto x = fs::weakly_canonical(fs::absolute(a));
    auto y = fs::weakly_canonical(fs::absolute(b));
#ifdef _WIN32
    return CompareStringOrdinal(x.c_str(), -1, y.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
    return x == y;
#endif
}

void check_outputs(const Command& cmd, const std::vector<fs::path>& files) {
    if (cmd.snapshot && cmd.report && same_file(*cmd.snapshot, *cmd.report)) {
        throw std::invalid_argument("report and snapshot paths overlap");
    }
    for (const auto& output : {cmd.snapshot, cmd.report}) {
        if (!output) {
            continue;
        }
        if (fs::exists(*output) && !fs::is_regular_file(*output)) {
            throw std::invalid_argument("output must be a file");
        }
        for (const auto& input : files) {
            if (same_file(*output, input)) {
                throw std::invalid_argument("output overlaps input");
            }
        }
    }
}

// A sibling staging directory reserves a unique name even across two CLI processes.
template <class Emit>
void save(const fs::path& path, Emit emit) {
    auto dir = path;
    dir += ".tracelab-tmp";
    if (!fs::create_directory(dir)) {
        throw std::runtime_error("output busy: " + bytes(dir.generic_u8string()));
    }
    const auto temp = dir / "content";

    struct Cleanup {
        fs::path file, dir;

        ~Cleanup() {
            std::error_code ec;
            fs::remove(file, ec);
            fs::remove(dir, ec);
        }
    } cleanup{temp, dir};

    std::ofstream out(temp, std::ios::binary);
    out.exceptions(std::ios::failbit | std::ios::badbit);
    emit(out);
    out.close();
    fs::rename(temp, path);
}

Report run(const Command& cmd, std::vector<fs::path> files) {
    std::stop_source cancel;
    std::mutex mutex;
    std::condition_variable_any cv;
    std::jthread timer;
    if (cmd.timeout == 0ms) {
        cancel.request_stop();
    } else if (cmd.timeout) {
        auto deadline = steady_clock::now() + *cmd.timeout;
        timer = std::jthread([&, deadline](std::stop_token stop) {
            std::unique_lock lock(mutex);
            cv.wait_until(lock, stop, deadline, [] { return false; });
            if (!stop.stop_requested()) {
                cancel.request_stop();
            }
        });
    }
    Observer observer;
    if (cmd.progress) {
        observer = [](Progress p) {
            std::cerr << std::format("session={} read={} processed={} matched={} done={}\n",
                                     p.session, p.read, p.processed, p.matched, p.done);
        };
    }
    return analyze(std::move(files), cmd.options, cancel.get_token(), std::move(observer));
}

Status dispatch(std::span<const std::string> args) {
    if (args.empty() || args.front() == "--help" || args.front() == "help") {
        std::cout << help;
        return Status::complete;
    }
    if (args.front() == "show-snapshot") {
        if (args.size() != 2 && !(args.size() == 4 && args[2] == "--timezone")) {
            throw std::invalid_argument("expected show-snapshot FILE [--timezone ZONE]");
        }
        Zone zone(args.size() == 4 ? args[3] : "UTC");
        std::ifstream in(path_from(args[1]), std::ios::binary);
        if (!in) {
            throw std::runtime_error("cannot open snapshot");
        }
        auto saved = load_snapshot(in);
        std::cout << "COMPLETE (statistics snapshot)\n"
                  << render_statistics(saved.stats, saved.query, zone);
        return Status::complete;
    }
    if (args.front() != "analyze") {
        throw std::invalid_argument("unknown command: " + args.front());
    }
    auto cmd = parse_command(args.subspan(1));
    auto files = discover(cmd.inputs, cmd.extension);
    check_outputs(cmd, files);
    Zone zone(cmd.timezone);
    auto report = run(cmd, std::move(files));
    auto text = render(report, zone);
    if (cmd.report) {
        save(*cmd.report, [&](auto& out) { out << text; });
    }
    if (cmd.snapshot && report.status != Status::cancelled) {
        save(*cmd.snapshot, [&](auto& out) { save_snapshot(report, out); });
    }
    std::cout << text;
    return report.status;
}
} // namespace

#ifdef _WIN32
Status cli(int argc, wchar_t** argv) {
    struct ConsoleEncoding {
        UINT old{GetConsoleOutputCP()};

        ConsoleEncoding() {
            if (old) {
                SetConsoleOutputCP(CP_UTF8);
            }
        }

        ~ConsoleEncoding() {
            if (old) {
                SetConsoleOutputCP(old);
            }
        }
    } encoding;
#else
Status cli(int argc, char** argv) {
#endif
    try {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) {
#ifdef _WIN32
            args.push_back(bytes(fs::path(argv[i]).u8string()));
#else
            args.emplace_back(argv[i]);
#endif
        }
        auto status = dispatch(args);
        std::cout.flush();
        if (!std::cout) {
            throw std::runtime_error("stdout write failed");
        }
        return status;
    } catch (const std::exception& e) {
        std::cerr << "tracelab: " << e.what() << '\n';
        return Status::failed;
    }
}
} // namespace tracelab
