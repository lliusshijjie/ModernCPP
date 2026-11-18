#include "tracelab/analysis.hpp"
#include "tracelab/queue.hpp"
#include "tracelab/schema.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <format>
#include <fstream>
#include <future>
#include <iostream>
#include <random>
#include <ranges>
#include <source_location>
#include <sstream>
#include <thread>

using namespace tracelab;
using namespace std::chrono_literals;

namespace {
void check(bool ok, std::source_location loc = std::source_location::current()) {
    if (!ok) {
        throw std::runtime_error(std::format("check failed at {}:{}", loc.file_name(), loc.line()));
    }
}

template <class F>
void rejects(F f, std::source_location loc = std::source_location::current()) {
    try {
        f();
    } catch (const std::exception&) {
        return;
    }
    check(false, loc);
}

struct Workspace {
    fs::path dir;

    Workspace() {
        std::random_device rand;
        for (int i = 0; i < 20; ++i) {
            dir =
                fs::temp_directory_path() / std::format("tracelab-test-{:x}-{:x}", rand(), rand());
            if (fs::create_directory(dir)) {
                return;
            }
        }
        throw std::runtime_error("cannot reserve test directory");
    }

    ~Workspace() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    fs::path write(fs::path name, std::string_view content) {
        auto path = dir / name;
        std::ofstream out(path, std::ios::binary);
        out.exceptions(std::ios::failbit | std::ios::badbit);
        out << content;
        out.close();
        return path;
    }
};

void input_checks() {
    static_assert(std::ranges::input_range<Generator<Record>>);
    static_assert(!std::ranges::forward_range<Generator<Record>>);
    static_assert(!std::copy_constructible<Generator<Record>>);
    static_assert(std::string_view(fields[5].data()) == "duration_us");
    auto utc = timestamp("2024-02-29T16:01:02.1234Z");
    check(timestamp("2024-03-01T00:01:02.123400+08:00") == utc);
    check(Zone("-05:30").display(utc) == "2024-02-29T10:31:02.123400-05:30");
    for (auto bad :
         {"2026-02-29T00:00:00Z", "2020-01-01T23:59:60Z", "2000-01-01T00:00:00.",
          "2000-01-01T00:00:00", "2000-01-01T00:00:00.0000001Z", "2000-01-01T00:00:00+01:60"}) {
        rejects([&] { timestamp(bad); });
    }
    check(valid_utf8(bytes(u8"文🌍")) && !valid_utf8("\xc0\x80") && !valid_utf8("\xed\xa0\x80") &&
          !valid_utf8("\xf4\x90\x80\x80") && !valid_utf8("\xe4\xb8"));
    std::string prefix = "2024-02-29T16:01:02.1234Z\tWARN\tservice\t42\t3\t0\t";
    auto event = parse({{0, 3}, prefix + bytes(u8"重试")});
    check(event.key.time == utc && event.pid == 42 && event.flags == 3 && event.duration == 0);
    for (std::string bad :
         {prefix + "\t", prefix + std::string(1, '\0'), prefix + "\r", prefix + "\xff",
          std::string("2024-01-01T00:00:00Z\tINFO\tcore\t1\t8\t-\tm")}) {
        rejects([&] { parse({{}, bad}); });
    }
    rejects([] { number<std::uint32_t>("4294967296"); });
    rejects([] { number<Count>("1x"); });

    Workspace w;
    auto p = w.write("test.log", "\xef\xbb\xbf\n" + prefix + "first\r\n\n" + prefix + "last");
    std::vector<Record> seen;
    auto original = records(p, 7, {});
    auto moved = std::move(original);
    check(original.begin() == original.end());
    for (auto& row : moved) {
        seen.push_back(std::move(row));
    }
    check(seen.size() == 2 && seen.front().source == Position{7, 2} &&
          seen.back().source == Position{7, 4});
    check(moved.begin() == moved.end() && seen.front().bytes.ends_with("first"));
    {
        auto short_read = records(p, 0, {});
        check((*short_read.begin()).source.line == 2);
    }
    check(fs::remove(p));
    auto missing = records(p, 0, {});
    rejects([&] { missing.begin(); });
    check(missing.begin() == missing.end());
    auto long_path = w.write("long.log", std::string(max_line + 1, 'x'));
    rejects([&] {
        auto too_long = records(long_path, 0, {});
        too_long.begin();
    });
    auto limit = w.write("limit.log", std::string(max_line, 'x') + "\r\n");
    for (const auto& row : records(limit, 0, {})) {
        check(row.bytes.size() == max_line);
    }
}

void analysis_checks() {
    Workspace w;
    std::string data;
    std::vector<Event> oracle;
    Options options{.query = {.minimum = Level::warn,
                              .components = {"net"},
                              .keyword = "keep",
                              .from = timestamp("2026-11-18T00:00:10Z"),
                              .to = timestamp("2026-11-18T00:00:50Z"),
                              .flags = 1,
                              .top = 11}};
    Count total{}, invalid{};
    for (auto i : std::views::iota(0, 6144)) {
        if (i % 37 == 0) {
            data += "bad\n";
            ++invalid;
            continue;
        }
        auto sec = (i * 13) % 60;
        auto level = i % 3 ? "WARN" : "INFO";
        auto component = i % 5 ? "net" : "ui";
        auto flags = i % 4;
        auto us = i % 17;
        auto message = i % 7 ? "keep" : "drop";
        auto line = std::format("2026-11-18T00:00:{:02}Z\t{}\t{}\t1\t{}\t{}\t{}", sec, level,
                                component, flags, us, message);
        data += line + '\n';
        if (sec >= 10 && sec < 50 && i % 3 && i % 5 && (flags & 1) && i % 7) {
            oracle.push_back(parse({{0, static_cast<Count>(i + 1)}, line}));
            total += us;
        }
    }
    auto p = w.write("data.log", data);
    auto files = discover({p, w.dir}, ".log");
    check(files.size() == 1);
    auto reference = analyze_serial(files, options);
    const auto& s = reference.summary;
    check(reference.status == Status::warnings && s.stats.processed == 6144 &&
          s.stats.invalid == invalid);
    check(s.stats.matched == oracle.size() && s.stats.total_us == total &&
          s.stats.components.at("net") == oracle.size());
    check(s.diagnostics.size() == 100 && s.diagnostics.back().source.line == 1 + 37 * 99);
    std::ranges::sort(oracle, {}, &Event::key);
    auto keys = std::views::transform(&Event::key);
    auto top_keys = oracle | std::views::take(11) | keys;
    check(std::ranges::equal(s.timeline | keys, top_keys));
    std::ranges::sort(oracle, [](auto& a, auto& b) {
        return a.duration != b.duration ? a.duration > b.duration : a.key < b.key;
    });
    check(std::ranges::equal(s.slowest | keys, top_keys));
    auto golden = render(reference, Zone("UTC"));
    for (auto n : {1, 2, 4, 8}) {
        options.workers = n;
        Progress last;
        auto parallel = analyze(files, options, {}, [&](Progress p) {
            check(p.read >= p.processed && p.processed >= p.matched);
            last = p;
        });
        check(last.done && last.processed == 6144 && last.matched == oracle.size());
        check(render(parallel, Zone("UTC")) == golden);
    }
    options.query.latest = true;
    options.query.top = 1;
    std::ranges::sort(oracle, {}, &Event::key);
    check(analyze(files, options).summary.timeline.front().key == oracle.back().key);
    options.query.top = 0;
    auto zero = analyze(files, options);
    check(zero.summary.timeline.empty() && zero.summary.slowest.empty() &&
          zero.summary.stats == s.stats);
    options.strict = true;
    rejects([&] { analyze(files, options); });
    options.strict = false;
    rejects([&] {
        analyze(files, options, {}, [](Progress) { throw std::runtime_error("observer failed"); });
    });
    std::stop_source stop;
    auto partial = analyze(files, options, stop.get_token(), [&](Progress p) {
        if (p.processed) {
            stop.request_stop();
        }
    });
    check(partial.status == Status::cancelled || partial.summary.stats == s.stats);
    check(partial.summary.stats.processed % 256 == 0);
    stop.request_stop();
    auto pre = analyze(files, options, stop.get_token());
    check(pre.status == Status::cancelled && pre.summary.stats.processed == 0);
    rejects([&] {
        std::ostringstream out;
        save_snapshot(pre, out);
    });
    auto empty = analyze({}, options);
    check(empty.status == Status::complete && empty.summary.stats.processed == 0);
    auto overflow = w.write("overflow.log",
                            "2026-01-01T00:00:00Z\tINFO\tc\t0\t0\t18446744073709551615\tx\n2026-01-"
                            "01T00:00:00Z\tINFO\tc\t0\t0\t1\ty");
    rejects([&] { analyze({overflow}, Options{}); });
}

void queue_checks() {
    for (bool full : {false, true}) {
        BoundedQueue<int> queue(1);
        if (full) {
            check(queue.push(7, {}));
        }
        std::stop_source stop;
        std::promise<void> entering;
        std::jthread peer([&] {
            entering.set_value();
            if (full) {
                check(!queue.push(8, stop.get_token()));
            } else {
                check(!queue.pop(stop.get_token()));
            }
        });
        entering.get_future().wait();
        stop.request_stop();
        peer.join();
        queue.close();
        if (full) {
            check(queue.pop({}) == 7);
        }
        check(!queue.pop({}) && !queue.push(1, {}));
    }
}

void file_order() {
    Workspace w;
    std::string line = "2026-11-18T12:00:00Z\tWARN\tcore\t5\t0\t7\t";
    auto z = w.write("z.log", line + "z");
    auto a = w.write("a.log", line + "a1\n" + line + "a2");
    auto files = discover({z, w.dir, a}, ".log");
    check(files.size() == 2 && files.front().filename() == "a.log");

    Options options;
    options.query.top = 2;
    auto report = analyze(files, options);
    check(report.summary.stats.matched == 3 && report.summary.timeline.at(0).message == "a1" &&
          report.summary.timeline.at(1).message == "a2");
    options.query.latest = true;
    auto latest = analyze(files, options);
    check(latest.summary.timeline.at(0).message == "z" &&
          latest.summary.timeline.at(1).message == "a2");
    check(latest.summary.slowest.at(0).message == "a1");
}

void snapshots() {
    Workspace w;
    auto p =
        w.write("a.log", "bad\n1970-01-01T00:00:00Z\tINFO\tx\t1\t7\t0\t" + std::string(510, 'a') +
                             bytes(u8"中文") + "\n1969-12-31T23:59:58Z\tERROR\ty\t2\t2\t-\tz");
    auto report = analyze({p}, Options{});
    auto preview = report.summary.slowest.front().message;
    check(valid_utf8(preview) && preview.size() <= 512 && preview.ends_with("..."));
    std::ostringstream output;
    save_snapshot(report, output);
    auto data = output.str();
    check(data.starts_with("TL20"));
    std::istringstream input(data);
    auto loaded = load_snapshot(input);
    check(loaded.stats == report.summary.stats && loaded.query == report.query);
    auto reject_bytes = [](std::string damaged) {
        rejects([&] {
            std::istringstream in(damaged);
            load_snapshot(in);
        });
    };
    for (std::size_t i = 0; i < data.size(); ++i) {
        reject_bytes(data.substr(0, i));
    }
    for (std::size_t i : {0, 4, 6, 11, 15}) {
        auto damaged = data;
        damaged.at(i) ^= 0x40;
        reject_bytes(damaged);
    }
    reject_bytes(data + "x");
    auto damaged = data;
    damaged.back() = 0x7f;
    reject_bytes(damaged);
    auto empty = analyze({}, Options{});
    std::stringstream empty_wire;
    save_snapshot(empty, empty_wire);
    check(load_snapshot(empty_wire).stats == Statistics{});
}

void zones() {
    Zone zone("America/New_York");
    auto a = zone.display(timestamp("2024-11-03T05:30:00Z"));
    auto b = zone.display(timestamp("2024-11-03T06:30:00Z"));
    check(a == "2024-11-03T01:30:00.000000-04:00" && b == "2024-11-03T01:30:00.000000-05:00");
    rejects([] { Zone("TraceLab/Unknown"); });
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            throw std::invalid_argument("expected test group");
        }
        std::string_view group = argv[1];
        if (group == "input") {
            input_checks();
        } else if (group == "analysis") {
            analysis_checks();
            file_order();
        } else if (group == "queue") {
            queue_checks();
        } else if (group == "snapshots") {
            snapshots();
        } else if (group == "zones") {
            zones();
        } else {
            throw std::invalid_argument("unknown group");
        }
        std::cout << group << " passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
