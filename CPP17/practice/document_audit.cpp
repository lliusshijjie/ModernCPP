// C++17 第 11～20 课综合练习。文档引用审计器。
// 在仓库根目录运行
// g++ -std=c++17 -Wall -Wextra -Wpedantic CPP17/practice/document_audit.cpp -o document_audit.exe
// .\document_audit.exe
// 阅读顺序：main → audit → scan_links → print_report → collect_metrics。

#include <algorithm>
#include <any>
#include <cstddef>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

struct Position {
    std::size_t line;
    std::size_t column;
};

struct Anchor {
    Position where;
    std::size_t incoming = 0;
};

struct Reference {
    Position where;
    std::string_view target; // [20] 只在 audit 扫描和解析期间借用源文本。
};

struct Malformed {
    Position where;
    std::string reason;
};
struct Duplicate {
    Position where;
    std::string name;
    Position original;
};
struct Missing {
    Position where;
    std::string name;
};
using Issue = std::variant<Malformed, Duplicate, Missing>; // [17] 封闭的诊断类型。

struct Report {
    std::map<std::string, Anchor, std::less<>> anchors;
    std::vector<Issue> issues;
    std::size_t references = 0;
    std::size_t resolved = 0;
};

std::string_view take_line(std::string_view& source) {
    const auto end = source.find('\n');
    auto line = source.substr(0, end);
    source.remove_prefix(end == std::string_view::npos ? source.size() : end + 1);
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
    }
    return line;
}

std::optional<std::string_view> parse_identifier(std::string_view text) {
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string_view::npos) {
        return std::nullopt;
    }
    const auto last = text.find_last_not_of(" \t");
    const auto name = text.substr(first, last - first + 1);
    constexpr std::string_view allowed =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
    if (name.find_first_not_of(allowed) != std::string_view::npos) {
        return std::nullopt;
    }
    return name; // [16/20] 有效标识符以视图返回，格式错误时为空。
}

void scan_anchor(std::string_view line, std::size_t number, Report& report) {
    const Position where{number, 1};
    if (const auto name = parse_identifier(line.substr(1)); name) { // [12]
        // [11] emplace 的结果直接拆成位置和插入状态。
        const auto [entry, inserted] = report.anchors.emplace(std::string(*name), Anchor{where});
        if (!inserted) {
            report.issues.push_back(Duplicate{where, entry->first, entry->second.where});
        }
    } else {
        report.issues.push_back(Malformed{where, "invalid anchor name"});
    }
}

void scan_links(std::string_view line, std::size_t number,
                std::vector<Reference>& references, Report& report) {
    std::size_t cursor = 0;
    for (;;) {
        const auto open = line.find("[[", cursor);
        if (open == std::string_view::npos) {
            break;
        }
        const Position where{number, open + 1};
        const auto begin = open + 2;
        const auto close = line.find("]]", begin);
        if (close == std::string_view::npos) {
            report.issues.push_back(Malformed{where, "unclosed reference"});
            break;
        }
        if (const auto name = parse_identifier(line.substr(begin, close - begin)); name) {
            references.push_back(Reference{where, *name});
        } else {
            report.issues.push_back(Malformed{where, "invalid reference name"});
        }
        cursor = close + 2;
    }
}

auto issue_position(const Issue& issue) {
    return std::visit([](const auto& detail) {
        const auto [line, column] = detail.where;
        return std::pair{line, column}; // [15] 推导 pair 的两个模板实参。
    }, issue);
}

Report audit(std::string_view source) {
    Report report;
    std::vector<Reference> references;
    // 第一遍收集定义与引用，源文本由调用方持有。
    for (std::size_t number = 1; !source.empty(); ++number) {
        const auto line = take_line(source);
        if (line == "#" || line.substr(0, 2) == "# ") {
            scan_anchor(line, number, report);
        } else {
            scan_links(line, number, references, report);
        }
    }

    // 第二遍查完整的锚点表，允许引用在定义之前出现。
    report.references = references.size();
    for (const auto& [where, target] : references) {
        if (auto found = report.anchors.find(target); found != report.anchors.end()) {
            ++found->second.incoming;
            ++report.resolved;
        } else {
            report.issues.push_back(Missing{where, std::string(target)});
        }
    }
    std::stable_sort(report.issues.begin(), report.issues.end(),
                     [](const Issue& left, const Issue& right) {
                         return issue_position(left) < issue_position(right);
                     });
    // 锚点和诊断中的标识符由 Report 持有，局部引用视图在此销毁。
    return report;
}

void print_issue(std::ostream& out, const Issue& issue) {
    std::visit([&out](const auto& detail) {
        const auto [line, column] = detail.where;
        out << line << ':' << column << ' ';
        using Detail = std::decay_t<decltype(detail)>;
        // [13/17] 每种诊断类型访问自己定义的字段。
        if constexpr (std::is_same_v<Detail, Malformed>) {
            out << detail.reason;
        } else if constexpr (std::is_same_v<Detail, Duplicate>) {
            out << "duplicate anchor " << detail.name
                << " (first at line " << detail.original.line << ')';
        } else {
            out << "missing anchor " << detail.name;
        }
        out << '\n';
    }, issue);
}

struct Totals {
    std::size_t anchors;
    std::size_t references;
    std::size_t resolved;
    std::size_t issues;

    void print(std::ostream& out) const {
        out << "anchors=" << anchors << ", references=" << references
            << ", resolved=" << resolved << ", issues=" << issues << '\n';
    }
};

Totals count_links(const Report& report) {
    return {report.anchors.size(), report.references, report.resolved, report.issues.size()};
}

struct BusiestAnchor {
    std::optional<std::string> name;
    std::size_t incoming = 0;

    void print(std::ostream& out) const {
        if (name) {
            out << "most referenced=" << *name << " (" << incoming << ")\n";
        } else {
            out << "most referenced=none\n";
        }
    }
};

BusiestAnchor find_busiest(const Report& report) {
    BusiestAnchor busiest;
    // map 按名称排序，引用数并列时保留字典序较小的名称。
    for (const auto& [name, anchor] : report.anchors) {
        if (anchor.incoming > busiest.incoming) {
            busiest = BusiestAnchor{name, anchor.incoming};
        }
    }
    return busiest;
}

struct Metric {
    std::any value; // [18] 分析函数可以提供各自的结果类型。
    void (*print)(std::ostream&, const std::any&);
};

template <class T>
Metric make_metric(T value) {
    // 存储和读取使用同一个 T，格式化函数随值一起保存。
    return {std::any{std::move(value)}, [](std::ostream& out, const std::any& stored) {
        std::any_cast<const T&>(stored).print(out);
    }};
}

template <class... Analyze>
std::vector<Metric> collect_metrics(const Report& report, Analyze... analyze) {
    std::vector<Metric> metrics;
    metrics.reserve(sizeof...(Analyze));
    (metrics.push_back(make_metric(analyze(report))), ...); // [14] 按传入顺序生成指标。
    return metrics;
}

void print_report(std::ostream& out, const Report& report) {
    // [19] optional 表示缺失，variant 表示已知诊断，any 保存开放的指标类型。
    const auto metrics = collect_metrics(report, count_links, find_busiest);
    for (const auto& [value, print] : metrics) {
        print(out, value);
    }
    for (const auto& issue : report.issues) {
        print_issue(out, issue);
    }
}

constexpr std::string_view demo_document = R"(# intro
See [[api]], [[usage]], [[ghost]].
# api
See [[usage]] and [[ intro ]].
# intro
# broken heading
Bad [[two words]] and [[missing
# usage
)";

int main(int argc, char* argv[]) {
    if (argc > 2 || (argc == 2 && std::string_view(argv[1]) != "--stdin")) {
        std::cerr << "Usage: document_audit [--stdin]\n";
        return 2;
    }

    std::string input;
    std::string_view source = demo_document;
    if (argc == 2) {
        std::string line;
        while (std::getline(std::cin, line)) {
            input += line;
            input.push_back('\n');
        }
        if (std::cin.bad() || !std::cin.eof()) {
            std::cerr << "Input read failed\n";
            return 2;
        }
        source = input; // 读完以后才借用 input，扫描期间不再扩容。
    }
    const auto report = audit(source);
    print_report(std::cout, report);
    return report.issues.empty() ? 0 : 1;
}
