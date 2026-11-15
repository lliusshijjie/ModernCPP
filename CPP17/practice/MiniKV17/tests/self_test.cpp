#include "minikv/shell.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <thread>

using namespace minikv;

namespace {

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void ok(Status status) {
    if (auto* error = std::get_if<Error>(&status)) {
        throw std::runtime_error(error->message);
    }
}

template <class F>
void throws(F function) {
    bool caught = false;
    try {
        function();
    } catch (const std::exception&) {
        caught = true;
    }

    check(caught, "expected exception");
}

struct TempDirectory {
    fs::path root = fs::weakly_canonical(fs::temp_directory_path());
    fs::path path;

    TempDirectory() {
        static std::atomic<unsigned> serial{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();

        for (unsigned attempt = 0; attempt < 100; ++attempt) {
            path = root / ("minikv17-test-" + scalar_text(stamp) + "-" + scalar_text(serial++));
            if (fs::create_directory(path)) {
                return;
            }
        }

        throw std::runtime_error("cannot create isolated test directory");
    }

    ~TempDirectory() {
        // 仅清理本测试独占创建的临时目录；先核对绝对父路径和固定前缀。
        if (path.parent_path() == root && path.filename().string().find("minikv17-test-") == 0) {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    }
};

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    check(bool(input), "read test file");

    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void write_file(const fs::path& path, const std::string& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();

    check(bool(output), "write test file");
}

Clock fixed_clock() {
    return [] { return std::int64_t{1000}; };
}

void parsing() {
    for (const auto* text : {"put k int +1", "put k int 12x", "put k int 9223372036854775808",
                             "put k bool 1", "put k string x 0", "put k string x -1", "get bad:key",
                             "get k extra", "put k string \"unterminated", "put k string \"x\"tail",
                             "put k string \"\\z\"", "put k int 1 2 extra"}) {
        check(std::holds_alternative<Error>(parse_command(text)), "bad input accepted");
    }

    auto empty = parse_command("put empty string \"\"");
    check(std::get<std::string>(std::get<Put>(std::get<Command>(empty)).value).empty(),
          "empty string");

    std::string source = "put key string \"line\\nnext\" 7";
    auto parsed = parse_command(source);
    source.assign(300, 'x');
    const auto& request = std::get<Put>(std::get<Command>(parsed));
    check(std::get<std::string>(request.value) == "line\nnext" && request.ttl == 7,
          "command ownership");

    check(!number<unsigned>("-1") && !number<int>(" 1") && !number<int>("1 "), "strict numbers");
    check(number<std::int64_t>("-9223372036854775808") == std::numeric_limits<std::int64_t>::min(),
          "min int");
}

void persistence() {
    static_assert(!std::is_move_constructible_v<Store> && !std::is_copy_constructible_v<Store>);

    TempDirectory dir;
    const std::string binary("a\0b\n", 4);

    {
        auto db = Store::open(dir.path, fixed_clock());
        ok(db.put("blob", binary));
        ok(db.put("min", std::numeric_limits<std::int64_t>::min()));
        ok(db.put("max", std::numeric_limits<std::int64_t>::max()));
        ok(db.put("flag", true));
        ok(db.put("gone", false));
        ok(db.erase("gone"));
        ok(db.rename("blob", "renamed"));
    }

    auto db = Store::open(dir.path, fixed_clock());
    check(std::get<std::string>(db.get("renamed")->value) == binary, "binary roundtrip");
    check(std::get<std::int64_t>(db.get("min")->value) == std::numeric_limits<std::int64_t>::min(),
          "min roundtrip");
    check(std::get<std::int64_t>(db.get("max")->value) == std::numeric_limits<std::int64_t>::max(),
          "max roundtrip");
    check(std::get<bool>(db.get("flag")->value) && !db.get("gone") && !db.get("blob"),
          "replay mutations");
    check(db.stats().replayed == 7 && db.stats().value_bytes == 21, "replay metrics");
}

void ttl() {
    TempDirectory dir;
    std::int64_t time = 100;
    const auto clock = [&] { return time; };

    {
        auto db = Store::open(dir.path, clock);
        ok(db.put("lease", std::string("token"), 5));
        time = 104;
        ok(db.rename("lease", "moved"));
        check(db.get("moved")->expires == 105, "rename extended TTL");
    }

    time = 105;
    auto db = Store::open(dir.path, clock);
    check(!db.get("moved") && db.scan().empty() && db.stats().keys == 0, "expiry boundary/restart");

    ok(db.put("moved", std::int64_t{8}));
    check(!db.get("moved")->expires, "overwrite TTL");

    check(std::holds_alternative<Error>(
              db.put("overflow", true, std::numeric_limits<std::int64_t>::max())),
          "TTL overflow");
}

void validation_and_snapshots() {
    TempDirectory dir;
    auto db = Store::open(dir.path, fixed_clock());
    ok(db.put("a", std::string(kMaxValue, 'x')));
    ok(db.put("b", false));
    const auto before = fs::file_size(dir.path / "store.wal");
    const auto rows = db.scan();
    auto value = db.get("a");

    check(std::holds_alternative<Error>(db.put("bad:key", true)), "bad key");
    check(std::holds_alternative<Error>(db.put("large", std::string(kMaxValue + 1, 'x'))),
          "large value");
    check(std::holds_alternative<Error>(db.rename("a", "b")), "rename conflict");
    check(std::holds_alternative<Error>(db.erase("missing")), "missing erase");
    check(fs::file_size(dir.path / "store.wal") == before, "failed validation appended");

    ok(db.rename("a", "a"));
    ok(db.put("a", std::string("changed")));
    std::get<std::string>(value->value) = "local edit";

    check(std::get<std::string>(rows[0].entry.value).size() == kMaxValue,
          "scan borrowed store memory");
    check(std::get<std::string>(db.get("a")->value) == "changed", "get borrowed store memory");
    check(db.scan("a").size() == 1 && db.scan("z").empty(), "prefix scan");
}

void compaction() {
    TempDirectory dir;
    {
        auto db = Store::open(dir.path, fixed_clock());
        for (int i = 0; i < 50; ++i) {
            ok(db.put("counter", std::int64_t{i}));
        }
        ok(db.put("other", true));
        ok(db.erase("other"));

        const auto before = fs::file_size(dir.path / "store.wal");
        ok(db.compact());
        check(fs::file_size(dir.path / "store.wal") < before && db.stats().compactions == 1,
              "compact size");

        ok(db.put("after", false));
    }

    auto db = Store::open(dir.path, fixed_clock());
    check(db.stats().replayed == 2 && std::get<std::int64_t>(db.get("counter")->value) == 49,
          "compact recovery");
    check(db.get("after").has_value() && !db.get("other"), "append after compact");
}

void torn_tail() {
    TempDirectory source;
    std::uintmax_t boundary = 0;

    {
        auto db = Store::open(source.path, fixed_clock());
        ok(db.put("first", true));
        boundary = fs::file_size(source.path / "store.wal");
        ok(db.put("second", std::string("payload")));
    }

    const auto complete = read_file(source.path / "store.wal");
    const auto second_size = complete.size() - static_cast<std::size_t>(boundary);

    for (auto cut :
         {std::size_t{1}, std::size_t{5}, std::size_t{15}, std::size_t{16}, second_size - 1}) {
        TempDirectory dir;
        write_file(dir.path / "store.wal",
                   complete.substr(0, static_cast<std::size_t>(boundary) + cut));
        {
            auto db = Store::open(dir.path, fixed_clock());
            check(db.get("first") && !db.get("second"), "torn tail replay");
            check(db.stats().discarded_tail == cut, "discarded count");
            ok(db.put("third", true));
        }

        auto db = Store::open(dir.path, fixed_clock());
        check(db.get("third") && db.stats().discarded_tail == 0, "append after repair");
    }
}

void corruption() {
    TempDirectory source;
    {
        auto db = Store::open(source.path);
        ok(db.put("key", std::string("hello")));
    }

    const auto original = read_file(source.path / "store.wal");

    for (int mode = 0; mode < 5; ++mode) {
        TempDirectory dir;
        auto damaged = original;
        if (mode == 0) {
            damaged.back() ^= 1;
        }
        if (mode == 1) {
            damaged[0] ^= 1;
        }
        if (mode == 2) {
            for (std::size_t i = 4; i < 8; ++i) {
                damaged[i] = 0;
            }
        }
        if (mode == 3) {
            damaged[4] ^= 1; // 合法范围内的长度位翻转，也必须拒绝
        }
        if (mode == 4) {
            damaged[12] ^= 1;
        }

        write_file(dir.path / "store.wal", damaged);
        throws([&] { auto db = Store::open(dir.path); });
        check(read_file(dir.path / "store.wal") == damaged, "corrupt journal silently repaired");
    }
}

void interrupted_switch() {
    TempDirectory dir;
    {
        auto db = Store::open(dir.path);
        ok(db.put("old", true));
    }

    fs::rename(dir.path / "store.wal", dir.path / "store.backup");
    write_file(dir.path / "store.next", "uncommitted garbage");
    {
        auto db = Store::open(dir.path);
        check(db.get("old").has_value(), "backup restore");
    }

    const auto stale_backup = read_file(dir.path / "store.wal");
    {
        auto db = Store::open(dir.path);
        ok(db.erase("old"));
        ok(db.put("new", true));
    }

    write_file(dir.path / "store.backup", stale_backup);
    {
        auto db = Store::open(dir.path);
        check(!db.get("old") && db.get("new"), "active preferred");
    }

    check(!fs::exists(dir.path / "store.next") && !fs::exists(dir.path / "store.backup"),
          "orphan cleanup");
}

void compaction_failure() {
    TempDirectory dir;
    {
        auto db = Store::open(dir.path);
        ok(db.put("kept", true));

        fs::create_directory(dir.path / "store.next");
        check(std::holds_alternative<Error>(db.compact()), "staging failure");
        ok(db.put("still_writable", true));

        fs::remove(dir.path / "store.next");
        fs::create_directory(dir.path / "store.backup");
        write_file(dir.path / "store.backup" / "blocker", "x");
        check(std::holds_alternative<Error>(db.compact()) && db.stats().faulted,
              "switch failure barrier");
        check(std::holds_alternative<Error>(db.put("blocked", true)), "write after fault");
    }

    fs::remove(dir.path / "store.backup" / "blocker");
    fs::remove(dir.path / "store.backup");
    auto db = Store::open(dir.path);
    check(db.get("kept") && db.get("still_writable") && !db.get("blocked"), "reopen after fault");
}

void empty_store() {
    TempDirectory dir;
    {
        auto db = Store::open(dir.path);
        ok(db.compact());
        check(db.stats().keys == 0, "empty stats");
    }

    auto db = Store::open(dir.path);
    check(db.scan().empty() && db.stats().replayed == 0, "empty compacted recovery");
}

void atomic_counter() {
    TempDirectory dir;
    {
        auto db = Store::open(dir.path, fixed_clock());
        ok(db.put("counter", std::int64_t{0}, 50));

        std::atomic<bool> failed{false};
        std::vector<std::thread> writers;
        for (int i = 0; i < 4; ++i) {
            writers.emplace_back([&] {
                try {
                    for (int n = 0; n < 25; ++n) {
                        ok(db.increment("counter", 1));
                    }
                } catch (...) {
                    failed = true;
                }
            });
        }
        for (auto& writer : writers) {
            writer.join();
        }

        check(!failed, "concurrent increment failed");
        const auto counter = db.get("counter");
        check(std::get<std::int64_t>(counter->value) == 100 && counter->expires == 1050,
              "atomic increment/TTL");

        ok(db.put("max", std::numeric_limits<std::int64_t>::max()));
        ok(db.put("min", std::numeric_limits<std::int64_t>::min()));
        ok(db.put("text", std::string("100")));
        const auto before = fs::file_size(dir.path / "store.wal");

        check(std::holds_alternative<Error>(db.increment("max", 1)), "positive overflow");
        check(std::holds_alternative<Error>(db.increment("min", -1)), "negative overflow");
        check(std::holds_alternative<Error>(db.increment("text", 1)), "counter type check");
        check(fs::file_size(dir.path / "store.wal") == before, "failed increment committed");

        ok(db.increment("created", -7));
        check(!db.get("created")->expires, "new counter TTL");
    }

    auto db = Store::open(dir.path, fixed_clock());
    check(std::get<std::int64_t>(db.get("counter")->value) == 100,
          "increment replay applied twice");
    check(std::get<std::int64_t>(db.get("created")->value) == -7, "missing counter initial value");
}

void capacity() {
    TempDirectory dir;
    auto db = Store::open(dir.path, fixed_clock());
    for (std::size_t n = 0; n < kMaxEntries; ++n) {
        ok(db.put("k" + scalar_text(n), false));
    }

    const auto before = fs::file_size(dir.path / "store.wal");
    check(std::holds_alternative<Error>(db.put("overflow", true)), "capacity limit");
    check(fs::file_size(dir.path / "store.wal") == before, "capacity failure appended");

    ok(db.put("k0", true));
    ok(db.erase("k1"));
    ok(db.increment("replacement", 1));
    check(db.stats().keys == kMaxEntries, "capacity replacement");
}

void concurrent_access() {
    TempDirectory dir;
    auto db = Store::open(dir.path, fixed_clock());

    std::atomic<bool> bad{false};
    std::atomic<bool> done{false};
    std::thread reader([&] {
        try {
            while (!done.load()) {
                const auto snapshot = db.scan("worker");
                const auto stats = db.stats();
                if (snapshot.size() > 4 || stats.keys > 4 || stats.writes < stats.keys) {
                    bad = true;
                }
            }
        } catch (...) {
            bad = true;
        }
    });
    std::vector<std::thread> writers;
    for (int worker = 0; worker < 4; ++worker) {
        writers.emplace_back([&, worker] {
            try {
                for (int i = 0; i < 30; ++i) {
                    ok(db.put("worker" + scalar_text(worker), std::int64_t{i}));
                }
            } catch (...) {
                bad = true;
            }
        });
    }

    for (auto& writer : writers) {
        writer.join();
    }
    done = true;
    reader.join();

    check(!bad && db.stats().keys == 4 && db.stats().writes == 120, "concurrent consistency");
    for (const auto& row : db.scan()) {
        check(std::get<std::int64_t>(row.entry.value) == 29, "lost update");
    }
}

void shell_session() {
    TempDirectory dir;
    auto db = Store::open(dir.path, fixed_clock());
    std::ostringstream output;
    Shell shell(db, output);
    shell.observe(SessionAudit{}, [](std::any& state, const CommandEvent& event) {
        check(std::any_cast<std::string>(&state) == nullptr, "any type probe");
        auto* audit = std::any_cast<SessionAudit>(&state);
        check(audit != nullptr, "any audit type");
        ++audit->commands;
        if (!event.success) {
            ++audit->errors;
        }
    });

    std::istringstream input("# comment\n\nput a string \"hi\\nthere\"\nrename a b\nget b\n"
                             "scan b\nget missing\nerase absent\nput x int "
                             "nope\nstats\ncompact\nhelp\nquit\nput skipped bool true\n");
    check(shell.run(input) == 1, "script error status");
    const auto* audit = std::any_cast<SessionAudit>(&shell.context());
    check(audit && audit->commands == 11 && audit->errors == 2, "observer audit");

    const auto text = output.str();
    check(text.find("VALUE string \"hi\\nthere\"") != std::string::npos, "shell string output");
    check(text.find("ERR 1: key missing") != std::string::npos &&
              text.find("ERR 2: invalid int64") != std::string::npos,
          "sequenced error output");
    check(!db.get("skipped") && db.get("b"), "quit/rename dispatch");

    std::istringstream counter_input("incr n -5\nget n\nput bytes string \"\\x00\\x7f\"\nquit\n");
    std::ostringstream counter_output;
    Shell counters(db, counter_output);
    check(counters.run(counter_input) == 0, "counter/hex command");
    check(counter_output.str().find("VALUE int -5") != std::string::npos, "incr command dispatch");
    check(std::get<std::string>(db.get("bytes")->value) == std::string("\0\x7f", 2), "hex escape");
    check(db.stats().reads >= db.stats().hits && db.stats().hits > 0, "read metrics");
}

} // namespace

int main() {
    std::cout << std::unitbuf;
    const std::pair<const char*, void (*)()> tests[] = {
        {"parsing", parsing},
        {"persistence", persistence},
        {"ttl", ttl},
        {"validation_and_snapshots", validation_and_snapshots},
        {"compaction", compaction},
        {"torn_tail", torn_tail},
        {"corruption", corruption},
        {"interrupted_switch", interrupted_switch},
        {"compaction_failure", compaction_failure},
        {"empty_store", empty_store},
        {"atomic_counter", atomic_counter},
        {"capacity", capacity},
        {"concurrent_access", concurrent_access},
        {"shell_session", shell_session}};

    unsigned passed = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            ++passed;
            line(std::cout, "PASS ", name);
        } catch (const std::exception& error) {
            line(std::cerr, "FAIL ", name, ": ", error.what());
        }
    }

    line(std::cout, "Tests: ", passed, "/", std::size(tests), " passed");
    return passed == std::size(tests) ? 0 : 1;
}
