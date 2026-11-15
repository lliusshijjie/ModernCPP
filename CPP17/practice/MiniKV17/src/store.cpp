#include "minikv/store.hpp"

#include <execution>
#include <limits>
#include <numeric>

namespace minikv {

Store::Store(fs::path directory, Clock clock)
    : journal_(std::move(directory)), clock_(std::move(clock)) {
    // 在对象发布前完成首次加锁，兼容本机 MinGW/winpthreads 出现的初次并发锁断言。
    // 参见 README 中的兼容性说明；不改变运行期的共享读/独占写模型。
    std::unique_lock initialize(data_mutex_);
    if (!clock_) {
        throw std::invalid_argument("clock is empty");
    }

    const auto time = now();
    auto recovery = journal_.recover();
    for (const auto& mutation : recovery.records) {
        replay(mutation, time);
    }

    if (data_.size() > kMaxEntries) {
        throw std::runtime_error("recovered store exceeds capacity");
    }

    metrics_.replayed = recovery.records.size();
    metrics_.discarded_tail = recovery.discarded;
}

std::int64_t Store::now() const {
    const auto time = clock_();
    if (time < 0) {
        throw std::runtime_error("clock must be nonnegative Unix seconds");
    }

    return time;
}

void Store::purge(std::int64_t time) {
    for (auto it = data_.begin(); it != data_.end();) {
        if (expired(it->second, time)) {
            it = data_.erase(it);
        } else {
            ++it;
        }
    }
}

void Store::replay(const Mutation& mutation, std::int64_t time) {
    std::visit(
        [&](const auto& record) {
            using T = std::decay_t<decltype(record)>;

            if constexpr (std::is_same_v<T, Write>) {
                data_.erase(record.key);
                if (!expired(record.entry, time)) {
                    data_.try_emplace(record.key, record.entry);
                }
            } else if constexpr (std::is_same_v<T, Erase>) {
                data_.erase(record.key);
            } else {
                auto node = data_.extract(record.from);
                if (!node.empty()) {
                    node.key() = record.to;
                    auto inserted = data_.insert(std::move(node));
                    if (!inserted.inserted) {
                        throw std::runtime_error("conflicting rename in journal");
                    }
                }
            }
        },
        mutation);
}

Status Store::put(std::string key, Value value, std::optional<std::int64_t> ttl) {
    if (!valid_key(key)) {
        return Error{"invalid key"};
    }
    if (value_size(value) > kMaxValue) {
        return Error{"value too large"};
    }

    // [23][15] 同时持有数据锁、指标锁，scoped_lock 负责避免多锁获取死锁，CTAD 省略模板参数。
    std::scoped_lock lock(data_mutex_, metrics_mutex_);
    if (journal_.faulted()) {
        return Error{"journal faulted; reopen required"};
    }

    const auto time = now();
    if (ttl && (*ttl <= 0 || *ttl > std::numeric_limits<std::int64_t>::max() - time)) {
        return Error{"invalid or overflowing TTL"};
    }

    purge(time);
    Entry entry{std::move(value), ttl ? std::optional<std::int64_t>(time + *ttl) : std::nullopt};

    return commit(std::move(key), std::move(entry));
}

Status Store::commit(std::string key, Entry entry) {
    if (data_.size() == kMaxEntries && data_.find(key) == data_.end()) {
        return Error{"store is full"};
    }

    Table staged;
    // [30] 预先分配节点，再写日志；提交之后的节点转移不需要再次分配内存。
    staged.try_emplace(std::move(key), std::move(entry));
    const auto& [owned_key, owned_entry] = *staged.begin(); // [11] 引用拆解，不复制值

    auto status = journal_.append(Write{owned_key, owned_entry});
    if (std::holds_alternative<Error>(status)) {
        return status;
    }

    auto node = staged.extract(staged.begin());
    data_.erase(node.key());
    data_.insert(std::move(node));
    ++metrics_.writes;

    return status;
}

std::optional<Entry> Store::get(std::string_view key) const {
    std::optional<Entry> result;

    {
        std::shared_lock lock(data_mutex_); // [23] 多个读者可并发；返回的是拥有数据的副本
        if (auto it = data_.find(key); it != data_.end() && !expired(it->second, now())) {
            result = it->second;
        }
    }

    // 读指标独立更新；绝不拿着 metrics_mutex_ 再等待 data_mutex_。
    std::lock_guard lock(metrics_mutex_);
    ++metrics_.reads;
    if (result) {
        ++metrics_.hits;
    }

    return result;
}

Status Store::increment(std::string key, std::int64_t delta) {
    if (!valid_key(key)) {
        return Error{"invalid key"};
    }

    std::scoped_lock lock(data_mutex_, metrics_mutex_);
    if (journal_.faulted()) {
        return Error{"journal faulted; reopen required"};
    }
    purge(now());

    std::int64_t current = 0;
    std::optional<std::int64_t> expires;
    if (const auto it = data_.find(key); it != data_.end()) {
        const auto* integer = std::get_if<std::int64_t>(&it->second.value);
        if (!integer) {
            return Error{"increment requires an integer"};
        }

        current = *integer;
        expires = it->second.expires;
    }

    if ((delta > 0 && current > std::numeric_limits<std::int64_t>::max() - delta) ||
        (delta < 0 && current < std::numeric_limits<std::int64_t>::min() - delta)) {
        return Error{"integer overflow"};
    }

    // 读-改-写在同一独占临界区内；重放的是最终值，不会重复执行增量。
    return commit(std::move(key), Entry{Value{current + delta}, expires});
}

Status Store::erase(std::string_view key) {
    if (!valid_key(key)) {
        return Error{"invalid key"};
    }

    std::scoped_lock lock(data_mutex_, metrics_mutex_);
    if (journal_.faulted()) {
        return Error{"journal faulted; reopen required"};
    }
    purge(now());

    const auto it = data_.find(key);
    if (it == data_.end()) {
        return Error{"key missing"};
    }

    auto status = journal_.append(Erase{it->first});
    if (std::holds_alternative<Error>(status)) {
        return status;
    }

    data_.erase(it);
    ++metrics_.writes;

    return status;
}

Status Store::rename(std::string_view from, std::string_view to) {
    if (!valid_key(from) || !valid_key(to)) {
        return Error{"invalid key"};
    }

    std::scoped_lock lock(data_mutex_, metrics_mutex_);
    if (journal_.faulted()) {
        return Error{"journal faulted; reopen required"};
    }
    purge(now());

    const auto source = data_.find(from);
    if (source == data_.end()) {
        return Error{"source missing"};
    }
    if (from == to) {
        return Ok{"unchanged"};
    }
    if (data_.find(to) != data_.end()) {
        return Error{"target exists"};
    }

    std::string new_key(to); // 在日志提交之前完成可能失败的分配
    auto status = journal_.append(Rename{source->first, new_key});
    if (std::holds_alternative<Error>(status)) {
        return status;
    }

    auto node = data_.extract(source);
    node.key().swap(new_key); // [30] 修改键，保留 value 和 TTL，无须复制值
    auto inserted = data_.insert(std::move(node));

    // 锁内已经检查目标不存在，插入不可能正常失败；保留诊断防止维护时破坏约束。
    if (!inserted.inserted) {
        throw std::logic_error("rename invariant violated");
    }

    ++metrics_.writes;

    return status;
}

std::vector<Row> Store::scan(std::string_view prefix) const {
    std::shared_lock lock(data_mutex_);
    const auto time = now();
    std::vector<Row> rows;

    for (auto it = data_.lower_bound(prefix); it != data_.end(); ++it) {
        const std::string_view key = it->first;
        if (key.substr(0, prefix.size()) != prefix) {
            break;
        }

        if (!expired(it->second, time)) {
            rows.push_back(Row{it->first, it->second});
        }
    }

    return rows;
}

Stats Store::stats() const {
    Stats result;
    std::vector<std::uint64_t> sizes;

    {
        std::scoped_lock lock(data_mutex_, metrics_mutex_);
        result = metrics_;
        result.faulted = journal_.faulted();

        const auto time = now();
        sizes.reserve(data_.size());
        for ([[maybe_unused]] const auto& [key, entry] : data_) {
            if (!expired(entry, time)) {
                sizes.push_back(value_size(entry.value));
            }
        }
    } // 锁外计算：执行策略的回调不拿锁、不分配、不执行 I/O

    result.keys = sizes.size();

#ifdef MINIKV17_PARALLEL
    const auto policy = std::execution::par;
#else
    const auto policy = std::execution::seq;
#endif

    // [25] 有界无符号整数加法，满足重排归约要求；par 允许并行，并不保证开线程。
    result.value_bytes =
        std::transform_reduce(policy, sizes.begin(), sizes.end(), std::uint64_t{0}, std::plus<>{},
                              [](std::uint64_t size) noexcept { return size; });

    return result;
}

Status Store::compact() {
    std::scoped_lock lock(data_mutex_, metrics_mutex_);
    if (journal_.faulted()) {
        return Error{"journal faulted; reopen required"};
    }
    purge(now());

    std::vector<Row> rows;
    rows.reserve(data_.size());
    for (const auto& [key, entry] : data_) {
        rows.push_back(Row{key, entry});
    }

    auto status = journal_.rewrite(rows);
    if (std::holds_alternative<Ok>(status)) {
        ++metrics_.compactions;
    }

    return status;
}

} // namespace minikv
