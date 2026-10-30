#pragma once

#include "signal_lab/model.h"

#include <iosfwd>
#include <map>
#include <mutex>
#include <shared_mutex>

namespace signal_lab {

std::vector<std::unique_ptr<Series>> read_series(std::istream& input);
Settings read_settings(std::istream& input);
std::string encode_settings(const Settings& settings);
void print_report(std::ostream& out, const Report& report);

// [06] 解析在锁外完成，整体替换在独占锁内完成；读者取独立快照。
class ConfigStore {
public:
    bool reload(const std::string& text);
    Settings snapshot() const;
    bool try_snapshot(Settings& result, std::chrono::milliseconds budget) const;

    // [09] 保留旧接口用于观察迁移诊断；正常示例不调用它。
    [[deprecated("use try_snapshot with std::chrono::milliseconds")]]
    bool try_snapshot_ms(Settings& result, int budget_ms) const {
        return try_snapshot(result, std::chrono::milliseconds{budget_ms});
    }

private:
    mutable std::shared_timed_mutex mutex_;
    Settings settings_;
};

class Catalog {
public:
    // 返回 true 表示新增，false 表示替换已有同名样本。
    bool upsert(Series series);
    std::unique_ptr<Series> snapshot(const char* name) const;
    std::vector<std::string> names() const;

private:
    mutable std::shared_timed_mutex mutex_;
    // [05A] find(const char*) 不必先构造临时 string；存储仍拥有 string 键。
    std::map<std::string, Series, std::less<>> entries_;
};

struct RosterComparison {
    bool same_order = false;
    bool same_members = false;
    std::size_t common_prefix = 0;
};

RosterComparison compare_rosters(const std::vector<std::string>& left,
                                 const std::vector<std::string>& right);

} // namespace signal_lab
