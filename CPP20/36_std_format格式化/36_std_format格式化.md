# 第 36 课 · std::format 格式化（C++20）

> 所需标准：**C++20**（`-std=c++20`，GCC 13 的 libstdc++ 开始支持 `<format>`，后续版本继续补齐细节和缺陷修正）｜ 讲解：本文件 ｜ 独立代码：`36_format_basics.cpp`
> 前置：无强前置；与第 48 课（chrono 格式化）配合食用

## 一、底层原理

### 1.1 旧世界的两条烂路

| | printf 家族 | iostream 操纵器 |
|---|---|---|
| 类型安全 | 无：`printf("%s", 42)` 编译过、运行 UB | 有 |
| 状态 | 无状态、格式即参数 | **有状态**：`setw` 只活一次、`setprecision` 永久生效 |
| 性能 | 取决于格式与实现 | 取决于格式、locale、缓冲与实现，需测量 |
| 可读性 | 说明符与实参分离，数 `%` 数到眼花 | 拼接式，对齐/精度写法冗长 |

`std::format`（源自 {fmt} 库）取两家之长：**类型安全 + 无状态 + 类 printf
的说明符 + Python 风格的位置字段**，一次调用返回 `std::string`。

### 1.2 编译期校验：格式串错误是编译错（P2216）

`std::format("{:d}", "abc")` **直接编译不过**——格式串是 `constexpr`
检查的函数实参：`basic_format_string<...>` 的 consteval 构造函数校验格式，
模板参数记录实参类型，格式串本身并非非类型模板参数。类型不符当场报错。
printf 的"运行到才崩"在这里不存在。代价：格式串必须是**编译期常量**——
运行期拼接的格式串要走 `std::vformat`，格式不合法时抛 `std::format_error`。
这里采用 P2216R3 对 C++20 的缺陷修正；最初发布的 C++20 接口并没有这项编译期保证。

### 1.3 常用替换域文法：`{[id][:[[fill]align][sign][#][0][width][.prec][L][type]]}`

```
{0:*^10.2f}
 └┬┘└┬┘└┬┘└┬─ 转换类型：d/x/o/b/f/e/g/s...
  │  │  │  └ .2 精度（浮点：小数位；字符串：截断）
  │  │  └─── 宽度（可用嵌套域 {:{}} 从实参取）
  │  └───── 对齐 < 左 / ^ 中 / > 右（默认数值右、字符串左）；
  │          fill 是除花括号外的一个字符，紧贴 align
  └──────── 位置编号：{0} 可复用："{0} vs {0}"
```

## 二、具体语法

```cpp
std::format("{} {}", "x", 42);          // "x 42"（自动编号）
std::format("{0}+{0}={1}", 2, 4);       // 位置与复用："2+2=4"
std::format("{:>8}", "ab");             // 右对齐宽 8
std::format("{:*^8}", "ab");            // 居中、fill 为 *
std::format("{:05}", -42);              // 零填充含符号：-0042
std::format("{:+.2f}", 3.14159);        // +3.14（显式正号）
std::format("{:#x} {:#b}", 255, 5);     // 0xff 0b101（备选形式）
std::format("{:10.3}", "abcdef");       // 先截断后补宽："abc       "
std::format("{:*^{}}", "ab", 6);        // 嵌套域：宽度来自实参 → "**ab**"
std::format("{{}}");                    // 字面花括号：转义
std::format("{}", true);                // "true"（bool 输出单词）
std::format("{:d}", true);              // 1（按整数对待）
```

规则细则：

- **自动编号与手动编号不可混用**：`"{} {0}"` 编译错
- fill 写在 align 前，不能是 `{` 或 `}`；支持 Unicode 的实现可使用
  一个 Unicode 码点作为填充字符，不能一概说 UTF-8 只能用单字节填充
- 字符串精度限制估算显示宽度，ASCII 示例表现为截断字符数，不能通用地
  当作字节上限；浮点 f/e 的精度是小数位数，g/G 或无 type 时为有效数字
- `0` 填充只对数值有意义；符号占宽度（-0042 宽度 5）
- `format_to(out, ...)` 写入迭代器；`format_to_n(out, n, ...)` 限制写入字符数
  （返回 `{out, size}`，size 是**未截断**的全长）；`formatted_size` 只算长度
- locale 隔离：默认**不进** locale（与 iostream 相反）；
  数字的本地化格式通常还需 `L`，如 `std::format(loc, "{:L}", value)`；
  只传 locale 而不加 `L` 不会自动为普通数字启用千分位

## 三、实际应用与最佳实践

### 3.1 场景：报表对齐（示例 ③）

`format("{:<12}{:>8.2f}", name, price)` 一行搞定左对齐名称、右对齐金额
——iostream 要 `left << setw(12) << name << right << fixed
<< setprecision(2) << setw(8)` 一长串，且状态外溢污染后续输出。

### 3.2 场景：有界输出（示例 ③）

`format_to_n` 最多写 n 个字符，但调用者必须保证目标确实有这么大；
它不会自动添加 `\0`。用作 C 字符串时要预留终止符空间，并通过返回的
`out` 指针补终止符；`size` 是未截断长度。`formatted_size` 可先量后写。

### 3.3 最佳实践清单

1. 新代码格式化一律 `std::format`；`printf` 冻结不再新增
2. 对齐模板里的"魔法宽度"用嵌套域参数化：`"{:>{}}"` + 列宽变量
3. bool 直接 `{}` 输出 true/false；要 0/1 再 `{:d}`
4. 拼接用户输入的格式串不合法（编译期常量要求）——动态格式走 `vformat`
5. 本地化数字显式传 locale 并使用 `L` 格式选项；默认格式不受全局 locale 影响

## 常见陷阱

1. **格式串不是常量**（变量拼接而来）→ 编译错——这是设计而非缺陷，
   用 `std::vformat(fmt_string, std::make_format_args(...))` 走运行期校验
2. `"{} {0}"` 混用自动/手动编号 → 编译错；同一串里二选一
3. `{:d}` 套字符串、`{:s}` 套 int → 编译错（不是运行时异常）——
   说明符与类型的一一对应是编译期契约
4. iostream 老习惯：`format` **无状态**，每次调用互不污染；
   反过来也别指望"设置一次精度后面都生效"
5. `format_to_n` 的 `size` 是全长不是写入长度——用它分配缓冲时先想清楚
   要哪个语义（目标为指针等随机访问迭代器时，写入长度可用 `out - begin`）
6. C++20 的 format **不认识容器**：`format("{}", std::vector{1,2})` 编译错，
   范围格式化是 C++23（第 59 课）

## 完整示例

见 `36_format_basics.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra "CPP20/36_std_format格式化/36_format_basics.cpp" -o run && ./run
```

## 练习

1. 打印九九乘法表：`format("{:>3} * {:>3} = {:>3}", ...)`，对齐整齐。
2. 用嵌套域参数化列宽：一个函数 `table(rows, width)`，宽度由调用方传。
3. `format_to_n` 截断实验：缓冲 4 字节格式化 "abcdefg"，打印写入数与
   返回的 size，验证两者的语义差异。
4. 把一段 printf 日志（含 %s/%d 混排）改写为 format，故意保留一处
   类型错配，对比两版的报错时机（运行时 UB vs 编译错）。

## 参考

- [P2216R3：格式检查与 basic_format_string](https://wg21.link/p2216r3)
- [C++20 草案：标准格式说明符（含 L）](https://timsong-cpp.github.io/cppwp/n4861/format.string.std)
- [libstdc++ 标准支持状态](https://gcc.gnu.org/onlinedocs/libstdc++/manual/status.html)
- 提案 P0645R10（文本格式化）/ P2216R3（格式串编译期校验）
- cppreference: [std::format](https://zh.cppreference.com/w/cpp/utility/format/format)
- cppreference: [格式说明符语法](https://zh.cppreference.com/w/cpp/utility/format/formatter#格式说明)
