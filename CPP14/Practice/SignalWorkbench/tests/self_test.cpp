#include "signal_lab/executor.h"
#include "signal_lab/tuple_tools.h"
#include "signal_lab/workbench.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>

namespace {

using namespace signal_lab;
using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(double left, double right) { return std::abs(left - right) < 1e-9; }

template <class Error, class Operation>
void expect_error(Operation&& operation) {
    bool caught = false;
    try { operation(); }
    catch (const Error&) { caught = true; }
    require(caught, "expected exception was not thrown");
}

// [03A] 检测的是函数声明；bool 范围和普通整数对象应在重载筛选时被排除。
template <class...> struct MakeVoid { using type = void; };
template <class Range, class = void> struct can_make_series : std::false_type {};
template <class Range>
struct can_make_series<Range, typename MakeVoid<decltype(
    make_series(std::string{}, std::declval<const Range&>()))>::type> : std::true_type {};

static_assert(can_make_series<std::vector<short>>::value, "integer range accepted");
static_assert(can_make_series<double[4]>::value, "native array accepted");
static_assert(!can_make_series<std::vector<bool>>::value, "bool samples rejected");
static_assert(!can_make_series<int>::value, "non-range rejected by substitution");
static_assert(std::is_final<Report>{}(), "report is a final value type");
static_assert(std::is_same<decltype(std::declval<const Series&>().samples()),
                           const std::vector<double>&>::value, "sample getter borrows");
static_assert(std::is_same<decltype(std::declval<Report&>().summary()), Summary&>::value,
              "mutable report getter preserves reference");
static_assert(std::is_same<decltype(std::declval<const Report&>().summary()),
                           const Summary&>::value, "const report getter preserves reference");

void test_analysis() {
    const int pulse[]{1, 0, -1, 0};
    const auto series = make_series("pulse", pulse);
    const auto report = analyze(*series, Settings{});
    require(report.summary().selected_count == 4 && near(report.summary().mean, 0.0),
            "pulse statistics differ");
    require(near(report.summary().rms, std::sqrt(0.5)), "pulse RMS differs");
    const auto& spectrum = std::get<Spectrum>(report.row());
    require(spectrum.computed && near(spectrum.bin1.real(), 2.0) &&
            near(spectrum.bin1.imag(), 0.0), "first DFT component differs");
    require(std::get<SignalName>(report.row()).value == "pulse", "typed name differs");

    const Settings amplified{"double", 1}; // 聚合初始化保留其余字段默认值。
    const auto twice = analyze(*series, amplified);
    require(near(twice.summary().rms, 2.0 * report.summary().rms), "gain was not applied");
    require(std::get<BatchTitle>(twice.row()).value == "double", "typed title differs");
    Settings stats_only{};
    stats_only.features = kStatistics;
    const auto without_spectrum = analyze(*series, stats_only);
    require(!std::get<Spectrum>(without_spectrum.row()).computed,
            "disabled spectrum was computed");
}

void test_proxy_and_snapshot() {
    Catalog catalog;
    catalog.upsert(Series{"pulse", {1.0, 0.0, -1.0, 0.0}});
    auto snapshot = catalog.snapshot("pulse");
    auto proxy = snapshot->selected(1);
    static_assert(std::is_same<decltype(proxy), std::vector<bool>::reference>::value,
                  "the proxy is returned by value, not as proxy&&");
    const bool before = proxy;
    proxy = false;
    require(before && !static_cast<bool>(proxy), "proxy and bool snapshot were confused");
    require(catalog.snapshot("pulse")->selected(1), "snapshot mutated catalog state");
    const auto report = analyze(*snapshot, Settings{});
    require(report.summary().selected_count == 3 &&
            near(report.summary().rms, std::sqrt(2.0 / 3.0)), "mask was ignored");
    require(near(std::get<Spectrum>(report.row()).bin1.real(), 2.0),
            "mask compressed the original DFT positions");
    require(!catalog.upsert(Series{"pulse", {9.0}}), "replacement was counted as insertion");
    require(snapshot->samples().size() == 4, "old snapshot changed after replacement");
    require(catalog.snapshot("pulse")->samples().size() == 1, "replacement was not published");
    require(!catalog.snapshot("missing"), "missing name should return null");
    expect_error<std::out_of_range>([&] { snapshot->selected(100) = true; });
    for (std::size_t i = 0; i < snapshot->samples().size(); ++i) snapshot->selected(i) = false;
    expect_error<std::invalid_argument>([&] { analyze(*snapshot, Settings{}); });
}

void test_configuration() {
    using namespace std::string_literals;
    ConfigStore store;
    Settings settings{"quoted \"title\" \\ path\nsecond line", 1};
    require(store.reload(encode_settings(settings)), "quoted settings did not round-trip");
    const auto original = encode_settings(store.snapshot());
    for (const auto& invalid : {"\"x\" -1 3 25", "\"x\" 4 3 25", "\"x\" 0 2 25",
                                "\"x\" 0 7 25", "\"x\" 0 3 -1", "\"x\" 0 3 5001",
                                "\"x\" 0 3 25 tail", "unquoted 0 3 25", "\"unfinished"}) {
        require(!store.reload(invalid) && encode_settings(store.snapshot()) == original,
                "invalid reload altered the live configuration");
    }
    Settings copy;
    require(store.try_snapshot(copy, 100ms), "timed snapshot failed");
    copy.title = "local change";
    require(encode_settings(store.snapshot()) == original, "snapshot leaked mutable state");
    require(!store.try_snapshot(copy, -1ms), "negative timeout was accepted");

    settings.title = "a\0b"s; // [08A] string 的长度保留中间的零字符。
    require(store.reload(encode_settings(settings)) && store.snapshot().title.size() == 3,
            "quoted string lost an embedded zero");
}

void test_input_validation() {
    std::istringstream valid("# comment\n\n\"a \\\"q\\\"\" 3 1 2 3\n\"b\" 1 -2\n");
    const auto series = read_series(valid);
    require(series.size() == 2 && series[0]->name() == "a \"q\"" &&
            series[1]->samples()[0] == -2.0, "valid input was not parsed");
    for (const auto& text : {"", "# no data", "a 1 2", "\"x\" 0", "\"x\" -1",
                             "\"x\" 4097", "\"x\" 1.0", "\"x\" 2 1", "\"x\" 1 abc",
                             "\"x\" 1 2 tail", "\"x\" 1 2\n\"x\" 1 3"}) {
        expect_error<std::invalid_argument>([&] {
            std::istringstream input(text);
            read_series(input);
        });
    }
    expect_error<std::invalid_argument>([] { Series("bad", {}); });
    expect_error<std::invalid_argument>([] {
        Series("bad", {std::numeric_limits<double>::infinity()});
    });
    expect_error<std::invalid_argument>([] { Series("bad", {1'000'001.0}); });
}

void test_range_algorithms() {
    const std::vector<std::string> a{"a", "b"};
    const auto equal = compare_rosters(a, a);
    require(equal.same_order && equal.same_members && equal.common_prefix == 2,
            "identical rosters differ");
    const auto longer = compare_rosters(a, {"a", "b", "c"});
    require(!longer.same_order && !longer.same_members && longer.common_prefix == 2,
            "prefix comparison was mistaken for full equality");
    const auto reordered = compare_rosters(a, {"b", "a"});
    require(!reordered.same_order && reordered.same_members && reordered.common_prefix == 0,
            "permutation comparison differs");
    require(!compare_rosters({"a", "a"}, {"a", "b"}).same_members,
            "permutation ignored duplicate counts");
    int values[]{1, 2, 3};
    *std::rbegin(values) = 9;
    require(*std::crbegin(values) == 9 && std::distance(std::rbegin(values), std::rend(values)) == 3,
            "reverse array access differs");
}

void test_executor_drain() {
    Executor executor(2);
    std::vector<std::future<Report>> futures;
    for (int i = 0; i < 16; ++i) {
        auto owned = make_series("job-" + std::to_string(i), std::vector<int>{1, 3});
        futures.push_back(executor.submit(std::move(owned), Settings{}));
        require(!owned, "unique snapshot ownership was not transferred");
    }
    executor.shutdown(); // 立即关闭，不预先 get；仍须完成所有已接收任务。
    executor.shutdown();
    for (auto& future : futures) {
        require(future.wait_for(0ms) == std::future_status::ready, "shutdown did not join workers");
        const auto report = future.get();
        require(near(report.summary().mean, 2.0), "drained task returned wrong data");
    }
    expect_error<std::logic_error>([&] {
        executor.submit(make_series("late", std::vector<int>{1}), Settings{});
    });
    expect_error<std::invalid_argument>([] { Executor invalid(0); });
    expect_error<std::invalid_argument>([] { Executor invalid(5); });
}

void test_worker_exception() {
    Executor executor(1);
    auto empty_selection = make_series("masked", std::vector<int>{1});
    empty_selection->selected(0) = false;
    auto failure = executor.submit(std::move(empty_selection), Settings{});
    auto success = executor.submit(make_series("next", std::vector<int>{7}), Settings{});
    executor.shutdown();
    expect_error<std::invalid_argument>([&] { failure.get(); });
    const auto report = success.get();
    require(near(report.summary().mean, 7.0), "one task exception stopped the worker");
    expect_error<std::invalid_argument>([&] { executor.submit(nullptr, Settings{}); });
}

void test_concurrent_config() {
    ConfigStore store;
    require(store.reload("\"even\" 0 1 10"), "cannot initialize concurrent configuration");
    std::promise<void> start;
    const auto gate = start.get_future().share();
    std::vector<std::future<void>> operations;
    operations.reserve(3);
    try {
        operations.push_back(std::async(std::launch::async, [&store, gate] {
            gate.wait();
            for (int i = 0; i < 200; ++i) {
                require(store.reload(i % 2 ? "\"even\" 0 1 10" : "\"odd\" 1 3 20"),
                        "concurrent configuration update failed");
            }
        }));
        for (int i = 0; i < 2; ++i) {
            operations.push_back(std::async(std::launch::async, [&store, gate] {
                gate.wait();
                for (int j = 0; j < 400; ++j) {
                    const auto value = store.snapshot();
                    const bool even = value.title == "even" && value.gain_index == 0 &&
                        value.features == 1 && value.lock_budget == 10ms;
                    const bool odd = value.title == "odd" && value.gain_index == 1 &&
                        value.features == 3 && value.lock_budget == 20ms;
                    require(even || odd, "reader observed a partially updated configuration");
                }
            }));
        }
    } catch (...) {
        start.set_value(); // 创建异步线程失败时，也放行已创建的线程。
        throw;
    }
    start.set_value();
    for (auto& operation : operations) operation.get();
}

void test_catalog_capacity() {
    Catalog catalog;
    for (std::size_t i = 0; i < kMaxSignals; ++i) {
        require(catalog.upsert(Series{"s" + std::to_string(i), {1.0}}), "insertion failed");
    }
    expect_error<std::length_error>([&] { catalog.upsert(Series{"overflow", {1.0}}); });
    require(!catalog.upsert(Series{"s0", {2.0}}), "full catalog refused a replacement");
    require(catalog.names().size() == kMaxSignals, "capacity accounting differs");
    expect_error<std::invalid_argument>([&] { catalog.snapshot(nullptr); });
}

void test_tuple_and_output() {
    std::vector<int> order;
    const auto row = std::make_tuple(1, 2, 3);
    visit_tuple(row, [&](auto value) { order.push_back(value); });
    require(order == std::vector<int>({1, 2, 3}), "tuple fields were visited out of order");
    int visits = 0;
    visit_tuple(std::tuple<>{}, [&](const auto&) { ++visits; });
    require(visits == 0, "empty tuple unexpectedly called visitor");
    const auto report = analyze(Series{"A \"quote\"", {2.0, 2.0}}, Settings{});
    std::ostringstream text;
    print_report(text, report);
    require(text.str().find("signal=\"A \\\"quote\\\"\" | batch=") == 0,
            "report name was not escaped or fields changed order");
    require(text.str().find("count=2,mean=2.000000,rms=2.000000") != std::string::npos,
            "statistics formatting differs");
}

} // namespace

int main() {
    const std::pair<const char*, void(*)()> tests[]{
        {"analysis and typed fields", test_analysis},
        {"proxy and independent snapshots", test_proxy_and_snapshot},
        {"quoted configuration and rejection", test_configuration},
        {"input validation", test_input_validation},
        {"dual-range algorithms", test_range_algorithms},
        {"executor drain and ownership", test_executor_drain},
        {"worker exception transport", test_worker_exception},
        {"concurrent configuration snapshots", test_concurrent_config},
        {"catalog capacity", test_catalog_capacity},
        {"tuple traversal and output", test_tuple_and_output}
    };
    const auto total = static_cast<std::size_t>(std::distance(std::cbegin(tests), std::cend(tests)));
    std::size_t failures = 0;
    for (const auto& test : tests) {
        try {
            test.second();
            std::cout << "PASS: " << test.first << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL: " << test.first << ": " << error.what() << '\n';
        }
    }
    std::cout << "Tests: " << (total - failures) << '/' << total << " passed\n";
    return failures == 0 ? 0 : 1;
}
