// 第 48 课示例：chrono 日历与时区（C++20）—— CPP20 篇收官课
//
// 结构：① 原理验证 —— 日历类型；字面量拼日期；有效性 ok()
//       ② 语法演示 —— sys_days 算术；hh_mm_ss；months 不重整
//       ③ 应用实战 —— 固定时刻跨时区换算（确定性输出）
//
// 编译运行：
//   g++ -std=c++20 -Wall -Wextra 48_calendar_timezones.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <chrono>
#include <format>
#include <iostream>

using namespace std::chrono;    // 日历字面量 2026y/11/16d 所在命名空间

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：operator/ 拼日期 —— 三种可读顺序，类型是 year_month_day
    year_month_day launch = 2026y / November / 16;
    std::cout << std::format("{:%Y-%m-%d}", launch) << '\n';   // 2026-11-16

    // 验证 2：星期几 —— sys_days（天粒度 time_point）桥接
    weekday wd{sys_days{launch}};
    std::cout << std::format("{:%A}", wd) << '\n';             // Monday

    // 验证 3：有效性显式问 —— 2 月 30 日"存在"但 !ok()
    year_month_day bad = 2026y / February / 30;
    std::cout << bad.ok() << ' ' << launch.ok() << '\n';       // false true

    // year/month 有效时，超范围的 day 按月首日加 day-1 计算；本例 -> 03-02
    // year/month 本身无效时结果未指定，不能推广成任意非法字段都能归一化
    std::cout << std::format("{:%Y-%m-%d}",
                             year_month_day{sys_days{bad}}) << '\n';  // 2026-03-02

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 天粒度算术：日期相减得天数
    auto days_left = sys_days{2026y / December / 31} - sys_days{launch};
    std::cout << days_left.count() << '\n';                    // 45

    // 2.2 闰年由类型系统回答：2024-02-29 合法、2026-02-29 非法
    std::cout << (2024y / February / 29).ok() << ' '
              << (2026y / February / 29).ok() << '\n';         // true false

    // 2.3 months/years 加法**不做重整**：1-31 加一月得 2-31（!ok，需自审）
    auto eom = 2026y / January / 31;
    auto next = eom + months{1};
    std::cout << next.ok() << ' '
              << std::format("{:%m-%d}", next) << '\n';        // false 02-31

    // 2.4 hh_mm_ss：时长拆时分秒（含小数尾数）
    hh_mm_ss hms{3h + 14min + 15s};
    std::cout << std::format("{:%H:%M:%S}", hms) << '\n';      // 03:14:15
    hh_mm_ss frac{9011ms};
    std::cout << std::format("{:%S}", frac) << '\n';           // 09.011

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 固定时刻跨时区：2026-11-16 12:00 UTC 的两地表现（输出确定）
    auto utc_noon = sys_days{launch} + 12h;
    std::cout << std::format("{:%Y-%m-%d %H:%M %Z}",
                             zoned_time{"Etc/UTC", utc_noon}) << '\n';
    // 2026-11-16 12:00 UTC
    std::cout << std::format("{:%Y-%m-%d %H:%M %Z}",
                             zoned_time{"Asia/Shanghai", utc_noon}) << '\n';
    // 2026-11-16 20:00 CST（+8）

    // 3.2 反向：本地时刻 -> sys_time；本例没有 DST 歧义
    // DST 切换附近可能抛 ambiguous_local_time / nonexistent_local_time
    auto sh_local = local_days{2026y / November / 16} + 20h;
    std::cout << std::format("{:%H:%M}",
                             zoned_time{"Asia/Shanghai", sh_local}
                                 .get_sys_time())
              << '\n';                                         // 12:00

    // 3.3 本机 current_zone()：本机系统时区（此处为 Etc/UTC——
    //     由 OS/环境决定；你的 Windows 可能返回别的，如实看待）
    // auto z = current_zone();  // 运行时依赖环境，本课不打印其值

    // 3.4 实战心法（对照 ①②③）：
    //   日历语义用 year_month_day；算术/存储换 sys_days（天粒度）
    //   months 加法后必查 ok()——"2-31"是合法的中间态不是错误
    //   时区换算交给 zoned_time + IANA 库名，别手写 ±8
    //   固定时间戳可减少环境漂移；命名时区规则仍取决于 tzdata 版本
    //   sys_time 不计闰秒；涉及闰秒时另用 utc_clock 等接口
    return 0;
}
