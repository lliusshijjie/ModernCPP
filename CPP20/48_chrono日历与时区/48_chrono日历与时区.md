# 第 48 课 · chrono 日历与时区（C++20）

> 所需标准：**C++20**（`-std=c++20`；时区库要求系统有 IANA tzdata，
> 本机 MSYS2 环境实测可用）｜ 讲解：本文件 ｜ 独立代码：`48_calendar_timezones.cpp`
> —— **CPP20 篇收官课**；下一课起进入 C++23（`-std=c++23`）
> 前置：第 08 课（chrono 与时间单位）；第 36 课（std::format——本课的输出引擎）

## 一、底层原理

### 1.1 时钟之后的另一半：日历与地理

C++11/14 的 chrono 精确而抽象：时间点是"数轴上的数"、时长是"带单位
的数"——但"2026 年 11 月 16 日是周几""上海此刻几点""1 月 31 日加一个
月"这些**人类历法问题**缺少直接的 chrono 接口，通常要结合 C 时间接口
或第三方库处理。C++20（P0355）补齐：**日历类型 + 时区数据库 +
格式化**三件套，chrono 成为完整的时间库。

### 1.2 日历类型：强类型拼图

`year / month / day / weekday / year_month / month_day /
year_month_day…`——每种半成品都是独立类型，`operator/` 把它们拼成
更完整的日期：

```cpp
auto d1 = 2026y / November / 16;   // year_month_day
auto d2 = November / 16 / 2026y;   // 同义（月/日/年 美式顺序也支持）
```

**关键设计**：构造函数保存字段，不自动拒绝不存在的日期——
`2026y/February/30` 是合法的 `year_month_day` 值，但 `ok()` 返回
false；当 year/month 有效但 day 超出当月范围时，转成 sys_days
按该月首日加 day-1 天计算（此例得 3 月 2 日）。若 year 或 month 本身
无效，转换结果未指定，不能把所有非法字段都当作可归一化日期。
"无效中间态"显式存在于类型系统中，而不是运行时异常。

### 1.3 sys_days：日历与数轴的桥

`sys_days` 是 system_clock 的天粒度 time_point，即不计闰秒的 Unix 时间轴；
它不等同于 utc_clock 的连续 UTC 时间轴。日历做**表示**，
sys_days 做**算术与存储**：

- 两个日期相减 → `duration`（天数）——闰年大小月全部自动正确
- `year_month_day{sys_days{...}}` 双向转换
- 时区换算的入口也是它（配 `hours` 偏移）

### 1.4 months/years 加法：不重整是特性

`ymd + months{1}`：1-31 → **2-31**（保持日字段 31，而非自动保持月末；不偷偷
截成 2-28）。真正表达"每月最后一天"可用 year_month_day_last；
若需求是"同一日号续期"，需自行检查 ok() 并决策——
银行计息、订阅续费这类对"同一天"敏感的业务，这个显式性是刚需。

### 1.5 时区：zoned_time 与 IANA 数据库

`zoned_time{"Asia/Shanghai", tp}` 把 UTC 时间点 + IANA 时区名
（按数据库记录的历史偏移/夏令时规则）合成"本地表示"；反向（`local_days` →
sys_time）也可转换，但夏令时可能产生重复或不存在的本地时间，
默认转换分别抛 ambiguous_local_time / nonexistent_local_time；
可先 get_info 检查，或按业务用 choose::earliest/latest 选择转换策略。
时区偏移/夏令时规则与闰秒属于不同机制；sys_time 不计闰秒，
需要闰秒语义时使用 utc_clock 等相关接口。

## 二、具体语法

```cpp
using namespace std::chrono;
year_month_day d = 2026y / November / 16;
weekday wd{sys_days{d}};                    // Monday
d.ok();                                     // 有效性
year_month_day{sys_days{2026y/February/30}};// 归一化：2026-03-02

auto n = sys_days{2026y/December/31} - sys_days{d};   // 45 天
auto m = 2026y/January/31 + months{1};     // 02-31：!ok()，不重整
(2024y/February/29).ok();                  // true（闰年由类型回答）

hh_mm_ss hms{3h + 14min + 15s};            // 03:14:15（含亚秒尾数）
hh_mm_ss f{9011ms};                        // %S -> 09.011

zoned_time sh{"Asia/Shanghai", sys_days{d} + 12h};     // UTC 12:00
std::format("{:%Y-%m-%d %H:%M %Z}", sh);   // 2026-11-16 20:00 CST
zoned_time back{"Asia/Shanghai",
                local_days{d} + 20h};      // 本地 -> UTC
current_zone();                            // 系统时区（依赖环境）
locate_zone("Europe/Berlin");              // 按 IANA 名查
```

规则细则：

- y/d 字面量来自 std::literals::chrono_literals，November 是 chrono 中的
  常量；相关名称可通过 using namespace std::chrono 访问
- `zoned_time` 打印用 std::format 的 chrono 扩展说明符
  （`%Y %m %d %H %M %S %A %Z`…，第 36 课格式串的日历方言）
- 时区库在运行时读 IANA 数据（Linux `/usr/share/zoneinfo`、
  MSYS2 自带）——找不到库时 `locate_zone` 抛异常
- `current_zone()` 返回值**随系统环境变**（本机实测 `Etc/UTC`）——
  测试代码别依赖它，用固定 `zoned_time{"时区名", 固定时间戳}`
- `hh_mm_ss` 的 `%S` 含小数部分（duration 精度决定位数）
- 闰秒（leap second）在 utc_clock 等相关层处理，sys_days 时间轴不计闰秒
- 日历意义的月/年加法应作用于相应日历类型；直接给 sys_time 加 months
  是加该 duration 定义的平均月长度，不是移动到下个日历月

## 三、实际应用与最佳实践

### 3.1 场景：到期日/账单计算（示例 ②）

"按月续费"用 `ymd + months{1}` 加 `ok()` 自审：2-31 这类不存在日期
按业务决策（截断/顺延/拒绝）——**决策点显式化**，比 mktime 的
静默归一化安全得多。

### 3.2 场景：跨时区排班（示例 ③）

会议系统：UTC 存储时间点，`zoned_time{"Asia/Shanghai"}` /
`{"America/New_York"}` 各渲各的——夏令时切换日的正确性由
IANA 规则描述；对本地时间输入仍需处理重复/不存在的情况。
测试用固定时间戳减少对当前时间/系统时区的依赖；若要求历史输出完全可复现，
还应固定 tzdata 版本。本课用 2026-11-16 12:00 UTC 演示。

### 3.3 最佳实践清单

1. 表示用日历类型、算术存储用 `sys_days`/`sys_seconds`——
   两个世界各干擅长的事
2. year_month_day 的 months/years 加法后检查 ok()；加天数应先转成
   sys_days 加 days 再转回，并留意目标类型可表示的范围
3. 按命名时区的规则转换时用 zoned_time + IANA 名；固定偏移不能替代
   一个有历史偏移/夏令时变化的命名时区
4. 跨平台检查时区库与数据是否可用；缺失时明确报错或采用业务认可的
   固定偏移方案，并标注该方案不提供命名时区的历史/夏令时规则
5. 日志与序列化统一 UTC（`sys_time`），展示层才转本地——
   服务器时区不该影响存储

## 常见陷阱

1. **2-30 / 2-31 悄悄顺延**：`year_month_day{sys_days{bad}}` 会得
   3 月的日子——用 ok() 先拦，别让归一化替你做业务决策
2. `+months{1}` 后不查 ok() 就入库：2-31 一路穿透到下游——
   "日期不存在"是中间态，出口前必须显式处理
3. `current_zone()` 写进测试/示例：输出随环境漂移（本机是
   Etc/UTC，你的机器可能是别的）——要确定就 `locate_zone` 指名道姓
4. 时区名写 `"China Standard Time"` 这类 Windows 显示名——
   IANA 名才是通货（`"Asia/Shanghai"`）；拼错抛异常
5. hh_mm_ss 的小数位由 Duration 的精度决定，不是固定九位；
   seconds 通常无小数、milliseconds 通常三位。控制格式可用 std::format
6. 闰年手判 `year % 4`——`year::is_leap()` 与
   `(2024y/February/29).ok()` 是现成答案

## 完整示例

见 `48_calendar_timezones.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra "CPP20/48_chrono日历与时区/48_calendar_timezones.cpp" -o run && ./run
```

## 练习

1. 算你出生日是周几（`weekday{sys_days{...}}`），再算下一个
   "生日落在周末"的年份（提示：逐年 + years{1} 循环查 weekday 的
   `iso_encoding()`，周六=6、周日=7）。
2. 写 `month_end(ymd)`：返回当月最后一天（下月 1 日减 `days{1}`），
   对 1~12 月验证。
3. "信用卡 31 日扣款"策略实验：1-31 逐月 +months{1} 打印 ok() 与
   归一化结果，设计你自己的截断策略并实现。
4. 用固定时间戳 2026-06-21 12:00 UTC 做"全球 Offices 时间表"：
   上海 / 柏林 / 纽约 / 悉尼 各打印一行（注意 6 月北半球夏令时）。

## 参考

- [C++20 草案：year_month_day 转换规则](https://timsong-cpp.github.io/cppwp/n4861/time.cal.ymd.members)
- [C++20 草案：本地时间歧义与不存在时间](https://timsong-cpp.github.io/cppwp/n4861/time.zone.members)
- [C++20 草案：system_clock 与不计闰秒的 sys_time](https://timsong-cpp.github.io/cppwp/n4861/time.clock.system)
- 提案 P0355R7（日历与时区扩展 chrono）
- cppreference: [日历类型](https://zh.cppreference.com/w/cpp/chrono#日历)
- cppreference: [std::zoned_time](https://zh.cppreference.com/w/cpp/chrono/zoned_time)
