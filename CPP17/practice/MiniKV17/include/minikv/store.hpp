#pragma once

#include "journal.hpp"

#include <map>
#include <mutex>
#include <shared_mutex>

namespace minikv {

class Store {
public:
    // [27] Store 含 mutex，不可复制/移动；返回同类型纯右值直接构造目标对象。
    static Store open(fs::path directory, Clock clock = wall_seconds) {
        return Store(std::move(directory), std::move(clock));
    }

    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    // 写操作：日志提交成功后发布内存状态。
    [[nodiscard]] Status put(std::string key, Value value,
                             std::optional<std::int64_t> ttl = std::nullopt);

    [[nodiscard]] Status increment(std::string key, std::int64_t delta);

    [[nodiscard]] Status erase(std::string_view key);

    [[nodiscard]] Status rename(std::string_view from, std::string_view to);

    // 查询返回拥有数据的副本，不向调用方暴露锁内引用。
    [[nodiscard]] std::optional<Entry> get(std::string_view key) const;

    [[nodiscard]] std::vector<Row> scan(std::string_view prefix = {}) const;

    [[nodiscard]] Stats stats() const;

    [[nodiscard]] Status compact();

private:
    using Table = std::map<std::string, Entry, std::less<>>;

    Store(fs::path directory, Clock clock);

    Journal journal_;
    Clock clock_;

    // 数据锁保护键值表；指标锁还允许 get 在释放数据锁后独立计数。
    mutable std::shared_mutex data_mutex_;
    mutable std::mutex metrics_mutex_;
    Table data_;
    mutable Stats metrics_;

    std::int64_t now() const;

    void purge(std::int64_t time);

    void replay(const Mutation& mutation, std::int64_t time);

    Status commit(std::string key, Entry entry); // 调用方必须已经持有两个锁
};

} // namespace minikv
