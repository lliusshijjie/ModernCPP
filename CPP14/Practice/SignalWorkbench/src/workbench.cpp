#include "signal_lab/workbench.h"
#include "signal_lab/tuple_tools.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <numeric>
#include <set>
#include <sstream>

namespace signal_lab {
namespace {

// quoted 的默认读取也接受无引号单词；本项目的文件格式要求显式双引号。
void read_quoted_name(std::istream& in, std::string& name) {
    in >> std::ws;
    if (in.peek() != '"' || !(in >> std::quoted(name))) {
        throw std::invalid_argument("expected a double-quoted name");
    }
}

void require_end(std::istream& in) {
    in >> std::ws;
    if (!in.eof()) throw std::invalid_argument("unexpected trailing input");
}

void print_cell(std::ostream& out, const SignalName& name) {
    out << "signal=" << std::quoted(name.value);
}
void print_cell(std::ostream& out, const BatchTitle& title) {
    out << "batch=" << std::quoted(title.value);
}
void print_cell(std::ostream& out, const Summary& summary) {
    out << "count=" << summary.selected_count << ",mean=" << summary.mean
        << ",rms=" << summary.rms;
}
void print_cell(std::ostream& out, const Spectrum& spectrum) {
    if (!spectrum.computed) {
        out << "bin1=disabled";
        return;
    }
    // 仅显示时把接近零的舍入误差归零；Report 中保留实际计算值。
    const auto display = [](double value) { return std::abs(value) < 1e-9 ? 0.0 : value; };
    out << "bin1=(" << display(spectrum.bin1.real()) << ','
        << display(spectrum.bin1.imag()) << ')';
}
void print_cell(std::ostream& out, std::chrono::microseconds elapsed) {
    out << "elapsed=" << elapsed.count() << "us";
}

} // namespace

void validate_settings(const Settings& settings) {
    using namespace std::chrono_literals;
    if (settings.title.empty() || settings.title.size() > 80 || settings.gain_index >= 4 ||
        (settings.features & kStatistics) == 0 ||
        (settings.features & ~kAllFeatures) != 0 ||
        settings.lock_budget < 0ms || settings.lock_budget > 5s) {
        throw std::invalid_argument("invalid settings");
    }
}

Series::Series(std::string name, std::vector<double> samples)
    : name_(std::move(name)), samples_(std::move(samples)), selected_(samples_.size(), true) {
    if (name_.empty() || name_.size() > 80 || name_.find('\0') != std::string::npos ||
        samples_.empty() || samples_.size() > kMaxSamples) {
        throw std::invalid_argument("invalid series name or sample count");
    }
    for (double value : samples_) {
        if (!std::isfinite(value) || std::abs(value) > 1'000'000.0) {
            throw std::invalid_argument("samples must be finite and bounded");
        }
    }
}

std::vector<std::unique_ptr<Series>> read_series(std::istream& input) {
    std::vector<std::unique_ptr<Series>> result;
    std::set<std::string, std::less<>> seen;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.size() > 65'536) throw std::invalid_argument("input line is too long");
        std::istringstream in(line);
        in >> std::ws;
        if (in.eof() || in.peek() == '#') continue;
        try {
            std::string name;
            read_quoted_name(in, name);
            long long count = 0;
            if (!(in >> count) || count <= 0 || count > static_cast<long long>(kMaxSamples)) {
                throw std::invalid_argument("invalid sample count");
            }
            // 避免把 "1.0" 误读成计数 1 和紧随其后的样本 .0。
            if (in.peek() != std::char_traits<char>::eof() &&
                !std::isspace(static_cast<unsigned char>(in.peek()))) {
                throw std::invalid_argument("sample count must be an integer token");
            }
            std::vector<double> samples(static_cast<std::size_t>(count));
            for (double& value : samples) {
                if (!(in >> value)) throw std::invalid_argument("missing or invalid sample");
            }
            require_end(in);
            if (!seen.insert(name).second) throw std::invalid_argument("duplicate signal name");
            if (result.size() == kMaxSignals) throw std::length_error("too many signals");
            result.push_back(make_series(std::move(name), samples));
        } catch (const std::invalid_argument& error) {
            throw std::invalid_argument("line " + std::to_string(line_number) + ": " + error.what());
        }
    }
    if (input.bad()) throw std::runtime_error("failed to read series input");
    if (result.empty()) throw std::invalid_argument("no signals in input");
    return result;
}

Settings read_settings(std::istream& input) {
    Settings settings;
    read_quoted_name(input, settings.title);
    long long gain = 0, features = 0, budget_ms = 0;
    if (!(input >> gain >> features >> budget_ms) || gain < 0 || gain >= 4 ||
        features < 0 || features > kAllFeatures || budget_ms < 0 || budget_ms > 5'000) {
        throw std::invalid_argument("invalid settings fields");
    }
    require_end(input);
    settings.gain_index = static_cast<std::size_t>(gain);
    settings.features = static_cast<unsigned>(features);
    settings.lock_budget = std::chrono::milliseconds{budget_ms};
    validate_settings(settings);
    return settings;
}

std::string encode_settings(const Settings& settings) {
    validate_settings(settings);
    std::ostringstream out;
    out << std::quoted(settings.title) << ' ' << settings.gain_index << ' '
        << settings.features << ' ' << settings.lock_budget.count();
    return out.str();
}

bool ConfigStore::reload(const std::string& text) {
    Settings candidate;
    try {
        std::istringstream in(text);
        candidate = read_settings(in);
    } catch (const std::invalid_argument&) {
        return false; // 失败时旧配置不变。
    }
    std::unique_lock<std::shared_timed_mutex> write(mutex_);
    settings_ = std::move(candidate);
    return true;
}

Settings ConfigStore::snapshot() const {
    std::shared_lock<std::shared_timed_mutex> read(mutex_);
    return settings_;
}

bool ConfigStore::try_snapshot(Settings& result, std::chrono::milliseconds budget) const {
    using namespace std::chrono_literals;
    if (budget < 0ms || budget > 5s) return false;
    std::shared_lock<std::shared_timed_mutex> read(mutex_, std::defer_lock);
    if (!read.try_lock_for(budget)) return false;
    result = settings_;
    return true;
}

bool Catalog::upsert(Series series) {
    // 先复制键；避免在同一次调用里既读取 series.name() 又移动 series。
    std::string key = series.name();
    std::unique_lock<std::shared_timed_mutex> write(mutex_);
    const auto found = entries_.find(key);
    if (found != entries_.end()) {
        found->second = std::move(series);
        return false;
    }
    if (entries_.size() == kMaxSignals) throw std::length_error("catalog is full");
    entries_.emplace(std::move(key), std::move(series));
    return true;
}

std::unique_ptr<Series> Catalog::snapshot(const char* name) const {
    if (!name) throw std::invalid_argument("null signal name");
    std::shared_lock<std::shared_timed_mutex> read(mutex_);
    const auto found = entries_.find(name); // [05A] 异构查找。
    if (found == entries_.end()) return nullptr;
    return std::make_unique<Series>(found->second); // 复制快照由调用方独占。
}

std::vector<std::string> Catalog::names() const {
    std::shared_lock<std::shared_timed_mutex> read(mutex_);
    std::vector<std::string> result;
    result.reserve(entries_.size());
    std::transform(entries_.cbegin(), entries_.cend(), std::back_inserter(result),
                   [](const auto& entry) { return entry.first; }); // [01] 泛型 lambda。
    return result;
}

RosterComparison compare_rosters(const std::vector<std::string>& left,
                                 const std::vector<std::string>& right) {
    // [05B] 所有双范围算法都传双方 end，长度不同也不会越界读取。
    const auto mismatch = std::mismatch(std::cbegin(left), std::cend(left),
                                        std::cbegin(right), std::cend(right));
    return RosterComparison{
        std::equal(std::cbegin(left), std::cend(left), std::cbegin(right), std::cend(right)),
        std::is_permutation(std::cbegin(left), std::cend(left),
                            std::cbegin(right), std::cend(right)),
        static_cast<std::size_t>(std::distance(std::cbegin(left), mismatch.first))
    };
}

Report analyze(const Series& series, const Settings& settings) {
    validate_settings(settings);
    const auto started = std::chrono::steady_clock::now();
    const double gain = kGains[settings.gain_index];
    auto calibrate = [gain](auto value) { return static_cast<double>(value) * gain; };
    std::vector<double> active;
    active.reserve(series.samples().size());
    for (std::size_t i = 0; i < series.samples().size(); ++i) {
        if (series.selected(i)) active.push_back(calibrate(series.samples()[i]));
    }
    if (active.empty()) throw std::invalid_argument("no selected samples");
    const double count = static_cast<double>(active.size());
    const double sum = std::accumulate(active.begin(), active.end(), 0.0,
        [](auto total, auto value) { return total + value; });
    const double square_sum = std::inner_product(active.begin(), active.end(), active.begin(), 0.0);
    const Summary summary{active.size(), sum / count, std::sqrt(square_sum / count)};

    Spectrum spectrum;
    if ((settings.features & kSpectrum) != 0) {
        // [08A] 只计算 DFT 的第 1 个频率分量；未选择的位置按零处理，时间位置不压缩。
        using namespace std::complex_literals;
        std::complex<double> phase = 1.0 + 0.0i;
        const auto step = std::polar(1.0, -2.0 * pi_v<double> /
                                    static_cast<double>(series.samples().size()));
        for (std::size_t i = 0; i < series.samples().size(); ++i) {
            if (series.selected(i)) spectrum.bin1 += calibrate(series.samples()[i]) * phase;
            phase *= step;
        }
        spectrum.computed = true;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started);
    return Report(ReportRow{SignalName{series.name()}, BatchTitle{settings.title},
                             summary, spectrum, elapsed});
}

void print_report(std::ostream& out, const Report& report) {
    std::ostringstream line;
    line << std::fixed << std::setprecision(6);
    // [01/03] 有状态初始化捕获 + 泛型 lambda，经 index_sequence 依次访问元组字段。
    auto visitor = [&line, first = true](const auto& cell) mutable {
        if (!first) line << " | ";
        first = false;
        print_cell(line, cell);
    };
    visit_tuple(report.row(), visitor);
    out << line.str() << '\n';
}

} // namespace signal_lab
