#pragma once

#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace signal_lab {

// [09] 同一份边界用于导入、构造和分析；数字分隔符只改善可读性。
constexpr std::size_t kMaxSamples = 4'096;
constexpr std::size_t kMaxSignals = 16;
constexpr unsigned kStatistics = 0b01;
constexpr unsigned kSpectrum = 0b10;
constexpr unsigned kAllFeatures = kStatistics | kSpectrum;

// [02] C++14 可以在 constexpr 函数里循环并修改本次求值创建的对象。
struct GainTable {
    double values[4] = {};
    constexpr double operator[](std::size_t i) const { return values[i]; }
};

constexpr GainTable make_gain_table() {
    GainTable table{};
    double gain = 1.0;
    for (std::size_t i = 0; i < 4; ++i) {
        table.values[i] = gain;
        gain *= 2.0;
    }
    return table;
}

constexpr auto kGains = make_gain_table();
static_assert(kGains[0] == 1.0 && kGains[3] == 8.0, "compile-time gains");

// [03/03A] integral_constant 的调用运算符、变量模板、_t 类型别名。
// 这里允许非 bool 的算术样本；char 也按其数值处理，而非解析为数字文本。
template <class T>
using is_sample = std::integral_constant<bool,
    std::is_arithmetic<std::remove_cv_t<T>>::value &&
    !std::is_same<std::remove_cv_t<T>, bool>::value>;

template <class T>
constexpr bool is_sample_v = is_sample<T>{}();

template <class T>
constexpr T pi_v = static_cast<T>(3.1415926535897932384626433832795L);

// [02A] 带默认成员初值的聚合；Settings{"night", 1} 只覆盖前两个字段。
struct Settings {
    std::string title = "C++14 lab";
    std::size_t gain_index = 0;
    unsigned features = kAllFeatures;
    std::chrono::milliseconds lock_budget{25};
};

void validate_settings(const Settings& settings);

// 拥有样本与选择掩码。长度在构造后保持不变，使代理失效规则容易追踪。
class Series {
public:
    Series(std::string name, std::vector<double> samples);
    const std::string& name() const { return name_; }

    // [07] 括号保留 const vector<double>&；禁止在临时对象上借出引用。
    decltype(auto) samples() const & { return (samples_); }
    const std::vector<double>& samples() const && = delete;

    // [07A] 可写访问返回 vector<bool>::reference 的值；const 访问返回 bool。
    decltype(auto) selected(std::size_t i) & { return selected_.at(i); }
    bool selected(std::size_t i) const & { return selected_.at(i); }
    bool selected(std::size_t) const && = delete;

private:
    std::string name_;
    std::vector<double> samples_;
    std::vector<bool> selected_;
};

// [03A/05B] SFINAE 检查范围的元素类型；非成员迭代器兼容数组与容器。
template <class Range>
using sample_type_t = std::decay_t<decltype(*std::cbegin(std::declval<const Range&>()))>;

template <class Range,
          std::enable_if_t<is_sample_v<sample_type_t<Range>>, int> = 0>
std::unique_ptr<Series> make_series(std::string name, const Range& source) {
    std::vector<double> values;
    for (auto it = std::cbegin(source); it != std::cend(source); ++it) {
        if (values.size() == kMaxSamples) {
            throw std::length_error("too many samples");
        }
        values.push_back(static_cast<double>(*it));
    }
    // [04] 由对象构造函数统一检查空输入、有限性与幅值边界。
    return std::make_unique<Series>(std::move(name), std::move(values));
}

// [10] 两个字段底层都是 string，语义薄包装让 get<T> 的类型键保持唯一。
struct SignalName { std::string value; };
struct BatchTitle { std::string value; };
struct Summary {
    std::size_t selected_count = 0;
    double mean = 0.0;
    double rms = 0.0;
};
struct Spectrum {
    bool computed = false;
    std::complex<double> bin1{};
};

using ReportRow = std::tuple<SignalName, BatchTitle, Summary, Spectrum,
                             std::chrono::microseconds>;

class Report final {
public:
    explicit Report(ReportRow row) : row_(std::move(row)) {}
    const ReportRow& row() const & { return row_; }
    const ReportRow& row() const && = delete;
    decltype(auto) summary() & { return std::get<Summary>(row_); }
    decltype(auto) summary() const & { return std::get<Summary>(row_); }
    const Summary& summary() const && = delete;

private:
    ReportRow row_;
};

Report analyze(const Series& series, const Settings& settings);

} // namespace signal_lab
