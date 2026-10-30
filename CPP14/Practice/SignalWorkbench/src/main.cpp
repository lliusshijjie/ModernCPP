#include "signal_lab/executor.h"
#include "signal_lab/workbench.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

int run(std::istream& input, std::istream& config_input, bool demonstrate_mask) {
    using namespace signal_lab;
    using namespace std::chrono_literals;
    using namespace std::string_literals;

    auto loaded = read_series(input);
    Catalog catalog;
    std::vector<std::string> input_order;
    for (auto& series : loaded) {
        input_order.push_back(series->name());
        catalog.upsert(std::move(*series));
    }
    // loaded 的指针仍拥有已移出的 Series 对象；这里统一销毁，不读取其旧内容。
    loaded.clear();

    ConfigStore config;
    const Settings imported = read_settings(config_input);
    if (!config.reload(encode_settings(imported))) throw std::runtime_error("cannot reload config");
    Settings settings;
    if (!config.try_snapshot(settings, imported.lock_budget)) {
        throw std::runtime_error("configuration snapshot timed out");
    }
    // config.try_snapshot_ms(settings, 25); // 取消注释，观察第 09 课的 deprecated 诊断。

    const auto ordered_names = catalog.names();
    const auto roster = compare_rosters(input_order, ordered_names);
    std::cout << std::boolalpha << "Config: " << encode_settings(settings) << '\n';
    std::cout << "Roster: same_order=" << roster.same_order
              << " same_members=" << roster.same_members
              << " common_prefix=" << roster.common_prefix << '\n';
    std::cout << "Reverse index:";
    for (auto it = std::crbegin(ordered_names); it != std::crend(ordered_names); ++it) {
        std::cout << ' ' << std::quoted(*it);
    }
    std::cout << '\n';

    Executor executor(2);
    std::vector<std::future<Report>> futures;
    for (const auto& name : ordered_names) {
        auto snapshot = catalog.snapshot(name.c_str()); // const char* 异构查找。
        // 对示例中的零值采样关闭选择；修改只作用于任务快照，不污染目录。
        if (demonstrate_mask && name == "pulse \"A\""s && snapshot->samples().size() > 1) {
            auto proxy = snapshot->selected(1); // 代理对象的值，不是 bool 快照。
            proxy = false;
            const auto original = catalog.snapshot(name.c_str());
            std::cout << "Mask edit: task=" << static_cast<bool>(proxy)
                      << " catalog=" << static_cast<const Series&>(*original).selected(1) << '\n';
        }
        futures.push_back(executor.submit(std::move(snapshot), settings));
    }
    executor.shutdown(); // 排空已接收任务并 join；析构时再次调用也是安全的。

    std::vector<Report> reports;
    for (auto& result : futures) reports.push_back(result.get());
    std::sort(reports.begin(), reports.end(), [](const auto& left, const auto& right) {
        return std::get<SignalName>(left.row()).value < std::get<SignalName>(right.row()).value;
    });
    for (const auto& report : reports) {
        print_report(std::cout, report);
        // [10] get<T> 返回存储字段的引用，两个 string 字段按语义包装区分。
        if (std::get<BatchTitle>(report.row()).value != settings.title) {
            throw std::runtime_error("batch title was not captured by value");
        }
    }
    // [08] 时间单位显式表达；这个值只用于展示单位换算，不额外等待。
    const auto budget_us = std::chrono::duration_cast<std::chrono::microseconds>(25ms);
    std::cout << "25ms = " << budget_us.count() << "us\n";
    std::cout << "Completed: " << reports.size() << '/' << ordered_names.size() << '\n';
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc == 1) {
            std::istringstream input(
                "\"steady\" 4 2 2 2 2\n"
                "\"alternating\" 4 1 -1 1 -1\n"
                "\"pulse \\\"A\\\"\" 4 1 0 -1 0\n");
            std::istringstream settings("\"C++14 \\\"lab\\\"\" 0 3 25");
            return run(input, settings, true);
        }
        if (argc == 3) {
            std::ifstream input(argv[1]);
            std::ifstream settings(argv[2]);
            if (!input || !settings) throw std::runtime_error("cannot open input files");
            return run(input, settings, false);
        }
        std::cerr << "Usage: signal_workbench [signals-file settings-file]\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
