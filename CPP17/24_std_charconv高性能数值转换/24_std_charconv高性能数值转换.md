# 第 24 课 · std::charconv：高性能数值转换（C++17）

> 所需标准：**C++17** ｜ 讲解：本文件 ｜ 独立代码：`24_charconv.cpp`
> 前置：第 20 课（string_view——charconv 的天然搭档）

## 一、底层原理

### 1.1 数值转换的"旧世界"有多贵

| 手段 | locale | 堆分配 | 异常 | 性能 |
|---|---|---|---|---|
| `atoi/strtol` 家族 | **敏感**（读全局 C locale） | 无 | errno 旧式 | 慢 |
| `std::sto*` / `std::to_string` | C++17 中受相应 C 转换函数的 locale 规则影响 | **可能分配** | **可能抛异常** | 取决于实现与场景 |
| `sscanf/sprintf` | 敏感 | 无 | 返回值式 | 慢 |
| **`to_chars/from_chars`** | **无关** | **零** | **零** | 面向高吞吐，具体收益需测量 |

charconv 是为 JSON/协议解析器设计的最底层原语：直线代码、无分配、无异常、
不带精度参数的浮点输出提供对应格式的最短往返表示；带精度时可能舍入。
精确往返保证以匹配的 from_chars 格式和同一实现为前提。

### 1.2 API 形态：result 结构体 + errc

不用异常、不用 errno，而是返回一个极简结果结构：

```cpp
struct to_chars_result   { char* ptr; std::errc ec; };   // ptr: 写入的末后
struct from_chars_result { const char* ptr; std::errc ec; };  // ptr: 消费到的位置
```

`ec == {}` 即成功；失败/部分解析靠 **ptr 位置**区分——这套"指针即进度"
的设计正适合配合 string_view 做就地解析（第 20 课的组合技）。

### 1.3 为什么"部分解析"是一等公民

`"123abc"` 对 `from_chars` 不是错误：解析出 123，ptr 停在 `'a'`。
解析器拿 ptr 继续走即可——**把"切分"与"转换"解耦**，
中间不需要任何字符串拷贝。

## 二、具体语法

```cpp
char buf[32];
auto [to, ec] = std::to_chars(buf, buf + sizeof buf, 42);      // 整数
auto [to2, ec2] = std::to_chars(buf, buf + sizeof buf, 3.14);  // double：最短往返
auto [to3, ec3] = std::to_chars(buf, buf + sizeof buf, 5 /*, 2*/);  // base 2..36

double d;
auto [from, ec4] = std::from_chars(p, end, d);        // 浮点
int n;
auto [from2, ec5] = std::from_chars(p, end, n /*, 16*/);  // 整数可指定进制
auto [to4, ec6] = std::to_chars(buf, buf+32, 3.14159,
                                std::chars_format::fixed, 2);   // 定点 2 位
```

规则细则：

- `from_chars` **不跳过前导空白**、**不接受前导 `+`**；整数只有有符号目标接受 `-`；
  16 进制**不吃 `0x` 前缀**（只认十六进制数字）
- 失败不修改目标值；`ec = errc::invalid_argument`（无可解析字符）或
  `errc::result_out_of_range`（溢出）
- 浮点格式：`chars_format::general/fixed/scientific/hex`，
  可带精度（`to_chars` 独有）
- 结果结构配合结构化绑定（第 11 课）食用最佳

## 三、实际应用与最佳实践

### 3.1 场景：就地解析协议/CSV（示例 ③-1）

string_view 定位字段 → `from_chars` 就地转 int——零拷贝零分配。
对照旧写法（substr 出 `string` 再 `stoi`）：需要复制字符，可能分配内存并抛异常。

### 3.2 场景：高性能序列化（示例 ③-2）

`to_chars` 写栈上 buffer + `fixed` 精度控制——替代 `sprintf`/`to_string`，
无 locale 抖动、无分配，延迟可预测。

### 3.3 最佳实践清单

1. 热路径数值转换一律 charconv；冷路径 `sto*` 图省事也行
2. 每次调用**必查** `ec` 与 ptr 位置："全部消费"要显式断言 `ptr == last`
3. 配 string_view 用：`(sv.data(), sv.data() + sv.size())` 就是一对迭代器
4. 输出定长小数用 `chars_format::fixed + precision`；默认格式是最短往返
5. 需要跳空白/带符号的宽松解析？先自己 trim（第 20 课 trim），再 from_chars

## 常见陷阱

1. 忘了"不跳空白、不吃 +"：`from_chars(" 42")` 直接 invalid_argument——
   预处理自己做
2. `"0xff"` 按 16 进制解析：读到 `0` 就停在 `x`——值是 **0** 且是"部分成功"，
   不是 255（示例 ② 现场演示）
3. `to_chars` 缓冲区不足 → `errc::value_too_large`，且**不保证**已写内容
4. 溢出（如 `long` 装 `"99999999999999999999"`）→ `result_out_of_range`，
   目标值保持不变；完整消费也不能替代 ec 检查
5. `std::errc` 是错误条件枚举，与 `error_code` 体系互通但**不是** bool——
   显式比较 `ec == std::errc::xxx` 或 `ec == {}`

## 完整示例

见 `24_charconv.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++17 -Wall -Wextra "CPP17/24_std_charconv高性能数值转换/24_charconv.cpp" -o run && ./run
```

## 练习

1. 写 `bool parse_ipv4(std::string_view, uint8_t out[4])`：用 from_chars
   逐段解析，处理"部分消费"（如 `"1.2.3.4x"` 应失败）。
2. 对拍 `std::to_string(0.1)` 与 `to_chars(0.1)` 的输出长度，体会"最短往返"。
3. 用 `chars_format::scientific` 写 123456.0，对比 `fixed` 的输出。
4. 把第 21 课示例里统计文件大小的 `to_string` 换成 `to_chars` 版本。

## 参考

- 提案 P0067R10（charconv；作者来自 rapidjson/CUDA 社区——性能基因的由来）
- cppreference: [std::to_chars](https://zh.cppreference.com/w/cpp/utility/to_chars)
- cppreference: [std::from_chars](https://zh.cppreference.com/w/cpp/utility/from_chars)

## 补充阅读

[24A · 完整解析与结果提交](../补充知识点/24_补充A_完整解析与结果提交/24_补充A_完整解析与结果提交.md)：区分转换成功、完整消费与业务范围，并在全部验证后更新结果。
