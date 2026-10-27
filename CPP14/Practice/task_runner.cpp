// C++14 综合练习：迷你批处理任务器（覆盖第 01～09 课）
//
// 场景：提交一批整数处理任务，读取共享配置，按权限排序、计算、重试，最后汇总。
// 计算与并发是真实执行；暂时失败由 fail_first 参数模拟，便于稳定观察重试。
// 每批最多 8 个任务，每个任务一个 std::async；这是教学模型，没有实现线程池。
//
// 从仓库根目录编译、运行（仅依赖标准库，最低 C++14）：
//   g++ -std=c++14 -Wall -Wextra -Wpedantic -pthread CPP14/Practice/task_runner.cpp -o task_runner
//   ./task_runner
//   ./task_runner --self-test
// Windows 下产物为 task_runner.exe，可以使用 .\task_runner.exe 运行。
//
// 课程对应：
// 01 泛型 lambda / 初始化捕获       -> sum_scaled / run_batch
// 02 constexpr 循环和局部修改       -> make_retry_delays
// 03 变量模板 / integer_sequence    -> is_sample_v / visit_fields
// 04 make_unique / 独占所有权       -> submit / 移动捕获 Job
// 05 exchange / quoted / _t / equal -> 消费待执行队列 / 配置 / 类型约束 / 自检
// 06 shared_timed_mutex / 读写锁    -> ConfigStore
// 07 decltype(auto) / 返回类型推导  -> Job::values / Report::row
// 08 chrono 字面量 / 时间单位      -> 100ms / milliseconds / 重试等待
// 09 二进制 / 数字分隔符 / 弃用    -> 权限位 / 4'096 / run_batch_ms
//
// 建议阅读顺序：main -> run_demo -> TaskRunner -> execute -> ConfigStore -> 模板工具。
// std::async、future、容器、RAII 等 C++11 内容作为基础设施使用。

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <shared_mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace practice {

using namespace std::chrono_literals;
using std::chrono::milliseconds;

constexpr std::size_t kMaxSamples = 4'096;
constexpr std::size_t kMaxJobs = 8;
constexpr unsigned kMaxRetries = 3;
constexpr unsigned kRun   = 0b001;
constexpr unsigned kSort  = 0b010;
constexpr unsigned kRetry = 0b100;
constexpr unsigned kAllPermissions = kRun | kSort | kRetry;

// [02] 自建固定数组，避免依赖 C++17 才 constexpr 化的 std::array 可写下标。
struct RetryDelays {
    unsigned ms[kMaxRetries] = {};
    constexpr unsigned operator[](unsigned i) const { return ms[i]; }
};

constexpr RetryDelays make_retry_delays(unsigned base) {
    RetryDelays result{};
    for (unsigned i = 0; i < kMaxRetries; ++i) {
        result.ms[i] = base;
        base *= 2;
    }
    return result;
}

constexpr auto kRetryDelays = make_retry_delays(2);
static_assert(kRetryDelays[0] == 2 && kRetryDelays[2] == 8,
              "retry table must be generated at compile time");

// [03/05] 自定义 _v 变量模板；这里没有使用 C++17 的 std::is_integral_v。
// 限制样本为最多 32 位整数，配合样本数量和倍率上限，保证 long long 求和不溢出。
template <class T>
constexpr bool is_sample_v = std::is_integral<T>::value &&
    !std::is_same<std::remove_cv_t<T>, bool>::value &&
    std::numeric_limits<T>::digits <= 32;

template <class T>
long long sum_scaled(const std::vector<T>& values, int scale) {
    static_assert(is_sample_v<T>, "samples must be non-bool integers of at most 32 bits");
    if (values.size() > kMaxSamples || scale < 1 || scale > 100) {
        throw std::invalid_argument("sample count or scale is out of range");
    }
    // [01] 先提升为 long long 再乘，避免 int 乘法先溢出；0LL 决定累加器类型。
    return std::accumulate(values.begin(), values.end(), 0LL,
        [scale](auto total, auto value) {
            return total + static_cast<long long>(value) * scale;
        });
}

// [03] index_sequence 是 integer_sequence<size_t, ...> 的别名。
// C++14 用数组初始化中的包展开保证从左到右访问，不使用 C++17 折叠表达式。
template <class Tuple, class Visitor, std::size_t... I>
void visit_fields(const Tuple& row, Visitor& visit, std::index_sequence<I...>) {
    const int ordered[] = {0, (static_cast<void>(visit(std::get<I>(row))), 0)...};
    static_cast<void>(ordered);
}

template <class... T>
void print_row(std::ostream& out, const std::tuple<T...>& row) {
    auto print = [&out, first = true](const auto& value) mutable {
        if (!first) out << " | ";
        first = false;
        out << value;
    };
    visit_fields(row, print, std::index_sequence_for<T...>{});
    out << '\n';
}

struct Settings {
    std::string label = R"(C++14 "lab" \ local)";
    int scale = 3;
    unsigned retries = 2;  // 重试次数，不含第一次执行。
};

bool valid(const Settings& settings) {
    return !settings.label.empty() && settings.label.size() <= 80 &&
           settings.scale >= 1 && settings.scale <= 100 &&
           settings.retries <= kMaxRetries;
}

// [05] 这是自定义的带引号配置格式，不是 CSV；quoted 负责空格、引号和反斜杠往返。
std::string encode(const Settings& settings) {
    std::ostringstream out;
    out << std::quoted(settings.label) << ' ' << settings.scale << ' ' << settings.retries;
    return out.str();
}

class ConfigStore {
public:
    // 解析和校验在锁外完成；失败不修改旧配置，成功则一次替换完整配置。
    bool reload(const std::string& text) {
        Settings candidate;
        int retries = 0;
        std::istringstream in(text);
        if (!(in >> std::quoted(candidate.label) >> candidate.scale >> retries)) return false;
        in >> std::ws;
        if (!in.eof() || retries < 0 || retries > static_cast<int>(kMaxRetries)) return false;
        candidate.retries = static_cast<unsigned>(retries);
        if (!valid(candidate)) return false;
        std::unique_lock<std::shared_timed_mutex> write(mutex_);
        settings_ = std::move(candidate);
        return true;
    }

    Settings snapshot() const {
        std::shared_lock<std::shared_timed_mutex> read(mutex_);
        return settings_;  // 返回副本；解锁后外界也不能修改内部配置。
    }

    bool snapshot_for(Settings& result, milliseconds budget) const {
        if (budget < 0ms) return false;
        std::shared_lock<std::shared_timed_mutex> read(mutex_, std::defer_lock);
        if (!read.try_lock_for(budget)) return false;
        result = settings_;
        return true;
    }

private:
    // [06] 同一线程不会重复持有同一把读写锁，也不尝试共享锁原地升级。
    mutable std::shared_timed_mutex mutex_;
    Settings settings_;
};

class Job {
public:
    Job(int job_id, std::string job_name, std::vector<int> input,
        unsigned allowed, unsigned failures = 0)
        : id(job_id), name(std::move(job_name)), permissions(allowed),
          fail_first(failures), values_(std::move(input)) {
        if (name.empty() || values_.size() > kMaxSamples ||
            (permissions & ~kAllPermissions) != 0) {
            throw std::invalid_argument("invalid job name, sample count or permissions");
        }
    }

    // [07] 括号让 decltype(auto) 推导为 vector<int>& / const vector<int>&。
    // 引用只在 Job 生存期内有效；每个 Job 只交给一个执行线程修改。
    decltype(auto) values() { return (values_); }
    decltype(auto) values() const { return (values_); }

    int id;
    std::string name;
    unsigned permissions;
    unsigned fail_first;

private:
    std::vector<int> values_;
};

static_assert(std::is_same<decltype(std::declval<Job&>().values()),
                           std::vector<int>&>::value, "getter must retain references");
static_assert(std::is_same<decltype(std::declval<const Job&>().values()),
                           const std::vector<int>&>::value, "const getter must be read-only");

enum class Status { done, denied, exhausted, config_timeout };

const char* status_name(Status status) {
    switch (status) {
    case Status::done: return "done";
    case Status::denied: return "denied";
    case Status::exhausted: return "exhausted";
    case Status::config_timeout: return "config-timeout";
    }
    return "unknown";
}

struct Report {
    int id;
    std::string name;
    Status status = Status::exhausted;
    unsigned attempts = 0;
    long long total = 0;
    std::vector<int> values;

    // [07] auto 推导按值返回的 tuple 类型；[03] 下游统一展开打印。
    auto row() const { return std::make_tuple(id, name, status_name(status), attempts, total); }
};

Report execute(Job& job, const ConfigStore& config, milliseconds budget) {
    Report report{job.id, job.name, Status::exhausted, 0, 0, {}};
    if ((job.permissions & kRun) == 0) {
        report.status = Status::denied;
        return report;
    }
    Settings settings;
    if (!config.snapshot_for(settings, budget)) {
        report.status = Status::config_timeout;
        return report;
    }
    // 每个任务只取一次快照，重试过程中参数保持一致；计算、睡眠都在锁外进行。
    const auto retries = (job.permissions & kRetry) != 0 ? settings.retries : 0u;
    for (unsigned attempt = 0; attempt <= retries; ++attempt) {
        report.attempts = attempt + 1;
        if (attempt < job.fail_first) {
            if (attempt < retries) {
                // [02/08] 编译期生成的退避表转成强类型时间，不使用裸整数 sleep。
                std::this_thread::sleep_for(milliseconds{kRetryDelays[attempt]});
            }
            continue;
        }
        if ((job.permissions & kSort) != 0) {
            auto& values = job.values();
            std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
                return a < b;
            });
        }
        report.total = sum_scaled(job.values(), settings.scale);
        report.values = std::move(job.values());
        report.status = Status::done;
        return report;
    }
    return report;
}

class TaskRunner {
public:
    // 控制接口由主线程调用；工作线程只持有各自的 Job 和共享 ConfigStore。
    void submit(std::unique_ptr<Job> job) {
        if (!job) throw std::invalid_argument("cannot submit a null job");
        if (pending_.size() >= kMaxJobs) throw std::length_error("batch is full");
        const auto duplicate = std::any_of(pending_.begin(), pending_.end(),
            [id = job->id](const auto& other) { return other->id == id; });
        if (duplicate) throw std::invalid_argument("duplicate job id");
        pending_.push_back(std::move(job));
    }

    std::size_t pending_count() const { return pending_.size(); }

    std::vector<Report> run_batch(const ConfigStore& config, milliseconds budget = 100ms) {
        if (budget < 0ms) throw std::invalid_argument("negative config wait budget");
        // [05] 一次性取走旧队列，并将成员替换为空队列。
        auto batch = std::exchange(pending_, JobList{});
        std::vector<std::future<Report>> futures;
        futures.reserve(batch.size());
        for (auto& job : batch) {
            // [01/04] unique_ptr 移进闭包；闭包拥有 Job，执行结束后自动释放。
            // 这样的闭包不可拷贝，不能放进要求可拷贝目标的 std::function。
            auto work = [owned = std::move(job), &config, budget] {
                return execute(*owned, config, budget);
            };
            static_assert(!std::is_copy_constructible<decltype(work)>::value,
                          "the worker owns a unique_ptr and must be move-only");
            futures.push_back(std::async(std::launch::async, std::move(work)));
        }
        std::vector<Report> reports;
        reports.reserve(futures.size());
        // get() 等待并接收异常；async future 析构也会等待尚未结束的任务。
        // 所以返回或异常离开时，都没有继续引用 config 的后台工作。
        for (auto& future : futures) reports.push_back(future.get());
        std::sort(reports.begin(), reports.end(), [](const auto& a, const auto& b) {
            return a.id < b.id;
        });
        return reports;
    }

    // [09] 保留旧接口作为迁移示例。取消 run_demo 中的注释可观察弃用警告。
    [[deprecated("use run_batch(config, std::chrono::milliseconds) instead")]]
    std::vector<Report> run_batch_ms(const ConfigStore& config, int budget_ms) {
        return run_batch(config, milliseconds{budget_ms});
    }

private:
    using JobList = std::vector<std::unique_ptr<Job>>;
    JobList pending_;
};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);  // Release 下也执行检查。
}

void run_demo(std::ostream* out) {
    ConfigStore config;
    TaskRunner runner;
    runner.submit(std::make_unique<Job>(3, "blocked", std::vector<int>{9}, kSort));
    runner.submit(std::make_unique<Job>(1, "sort-and-retry", std::vector<int>{3, 1, 2},
                                       kRun | kSort | kRetry, 1));
    runner.submit(std::make_unique<Job>(5, "no-retry-permission", std::vector<int>{5}, kRun, 1));
    runner.submit(std::make_unique<Job>(2, "plain-sum", std::vector<int>{10, -2, 7}, kRun));
    runner.submit(std::make_unique<Job>(4, "always-fails", std::vector<int>{8},
                                       kRun | kRetry, 10));
    const auto reports = runner.run_batch(config, 100ms);
    // const auto old_api = runner.run_batch_ms(config, 100); // 取消注释，观察 deprecated。
    require(reports.size() == 5 && runner.pending_count() == 0, "batch was not consumed");
    require(runner.run_batch(config).empty(), "a consumed batch ran twice");
    require(reports[0].status == Status::done && reports[0].attempts == 2 &&
            reports[0].total == 18, "sort/retry task produced an incorrect result");
    const std::vector<int> sorted{1, 2, 3};
    // [05] 四迭代器版本还会检查长度，避免只比较前缀。
    require(std::equal(sorted.begin(), sorted.end(), reports[0].values.begin(),
                       reports[0].values.end()), "sorted result differs");
    require(reports[1].status == Status::done && reports[1].attempts == 1 &&
            reports[1].total == 45 && reports[1].values == std::vector<int>({10, -2, 7}),
            "plain task must preserve sample order");
    require(reports[2].status == Status::denied && reports[2].attempts == 0,
            "permission check happened too late");
    require(reports[3].status == Status::exhausted && reports[3].attempts == 3,
            "retry limit must allow one initial attempt plus two retries");
    require(reports[4].status == Status::exhausted && reports[4].attempts == 1,
            "a job retried without permission");
    if (out) {
        *out << "Config: " << encode(config.snapshot()) << '\n';
        *out << "id | job | status | attempts | scaled-sum\n";
        for (const auto& report : reports) print_row(*out, report.row());
        const auto done = std::count_if(reports.begin(), reports.end(), [](const auto& report) {
            return report.status == Status::done;
        });
        *out << "Completed: " << done << '/' << reports.size() << "\nDemo checks: PASS\n";
    }
}

void run_self_tests() {
    run_demo(nullptr);
    ConfigStore config;
    const auto original = encode(config.snapshot());
    require(config.reload(original) && encode(config.snapshot()) == original,
            "quoted configuration failed to round-trip");
    for (const auto& invalid : {"\"x\" 0 2", "\"x\" 3 -1", "\"x\" 3 4",
                                "\"x\" 3 2 junk", "\"unfinished", "\"\" 3 2"}) {
        require(!config.reload(invalid) && encode(config.snapshot()) == original,
                "invalid configuration changed live state");
    }
    Settings copy;
    require(config.snapshot_for(copy, 100ms), "timed snapshot failed on an idle store");
    copy.label = "only a copy";
    require(encode(config.snapshot()) == original, "snapshot leaked mutable internal state");
    require(!config.snapshot_for(copy, -1ms), "negative wait budget was accepted");
    require(sum_scaled(std::vector<int>{}, 3) == 0, "empty input must sum to zero");
    require(sum_scaled(std::vector<short>{1, -2, 3}, 2) == 4, "generic integer input failed");
    require(sum_scaled(std::vector<unsigned>{4'000'000'000u}, 100) == 400'000'000'000LL,
            "multiplication did not widen before computing");
    Job local(1, "reference-check", {2, 1}, kRun);
    local.values()[0] = 7;
    require(static_cast<const Job&>(local).values()[0] == 7, "getter returned a copy");
    TaskRunner timed_runner;
    timed_runner.submit(std::make_unique<Job>(1, "negative-budget", std::vector<int>{}, kRun));
    bool rejected = false;
    try { timed_runner.run_batch(config, -1ms); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected && timed_runner.pending_count() == 1,
            "invalid budget consumed the pending batch");

    // 一个写线程交替更新相关字段；三个读线程只能读到完整的同一版本。
    require(config.reload("\"even\" 2 0"), "cannot initialize concurrent test");
    std::promise<void> start;
    const auto gate = start.get_future().share();
    std::vector<std::future<void>> workers;
    workers.reserve(4);
    try {
        workers.push_back(std::async(std::launch::async, [&config, gate] {
            gate.wait();
            for (int i = 0; i < 300; ++i) {
                require(config.reload(i % 2 == 0 ? "\"odd\" 3 1" : "\"even\" 2 0"),
                        "concurrent reload failed");
            }
        }));
        for (int i = 0; i < 3; ++i) {
            workers.push_back(std::async(std::launch::async, [&config, gate] {
                gate.wait();
                for (int j = 0; j < 500; ++j) {
                    const auto snapshot = config.snapshot();
                    require((snapshot.label == "even" && snapshot.scale == 2 && snapshot.retries == 0) ||
                            (snapshot.label == "odd" && snapshot.scale == 3 && snapshot.retries == 1),
                            "a reader observed a torn configuration");
                }
            }));
        }
    } catch (...) {
        // 即便创建线程失败，也先放行已启动的任务，防止 future 析构等不到它们退出。
        start.set_value();
        throw;
    }
    start.set_value();
    for (auto& worker : workers) worker.get();
}

} // namespace practice

int main(int argc, char* argv[]) {
    try {
        if (argc == 1) {
            practice::run_demo(&std::cout);
        } else if (argc == 2 && std::string(argv[1]) == "--self-test") {
            practice::run_self_tests();
            std::cout << "Self-tests: PASS\n";
        } else {
            std::cerr << "Usage: task_runner [--self-test]\n";
            return 2;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
