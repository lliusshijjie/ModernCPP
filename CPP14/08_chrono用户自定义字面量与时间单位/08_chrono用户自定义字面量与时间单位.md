# 第 08 课 · chrono 用户自定义字面量与时间单位（C++14）

> 所需标准：C++14 ｜ 讲解：本文件 ｜ 独立代码：`08_chrono_literals.cpp`

## 一、底层原理

### 1.1 单位编码在类型里：duration<Rep, Period>

chrono 的核心不是一个个类，而是一个模板：

```cpp
template <class Rep, class Period> struct duration;   // 数值 + 编译期周期(分数)
using hours    = duration<int64-type, ratio<3600, 1>>;
using minutes  = duration<int64-type, ratio<60, 1>>;
using seconds  = duration<int64-type, ratio<1, 1>>;
```

所以"2 小时 + 30 分钟"是**两个不同类型的对象相加**——编译器按各自的
`ratio` 换算，结果的 common_type 取**更细的一方**（唯一无精度损失的方向）。
单位错误从此是编译错误，而不是深夜排查的线上 bug。

### 1.2 UDL 机制：编译器认识的后缀

用户自定义字面量（UDL）是 C++11 的语言机制，C++14 的 chrono 后缀是它
第一个大规模应用。`operator""h` 有两个重载：

| 字面量形式 | 调用的重载 | 得到的类型 |
|---|---|---|
| `2h`（整数） | `operator""h(unsigned long long)` | `chrono::hours` |
| `1.5h`（浮点） | `operator""h(long double)` | `duration<long double, ratio<3600>>` |

`2h` 和 `1.5h` **类型不同**——整数 rep 与浮点 rep 是两套 duration。

### 1.3 旧写法危险在哪

```cpp
sleep_for(100);                        // 编译错误：没有单位
sleep_for(std::chrono::milliseconds(100));  // 冗长，但至少有单位
sleep_for(100);  // /* 100 = 毫秒 */    // 注释单位——注释会过期，类型不会
```

字面量把单位写进代码本身：`100ms` 无法被误读，换单位是改代码而非改注释。

## 二、具体语法

### 2.1 六个标准后缀

```cpp
using namespace std::chrono_literals;   // h / min / s / ms / us / ns
auto d = 2h + 30min + 90s;              // 单位混算随意写
```

### 2.2 规则细则

- 整数字面量 → 整数 rep 的 duration；浮点字面量 → `long double` rep
- 混单位运算自动取更细单位；**变粗必须 `duration_cast`**（截断发生在这一步，可见）
- 标准后缀不带下划线（编译器保留区）；**自定义 UDL 必须以下划线开头**（`_km`）
- `using namespace std::chrono_literals;` 建议放函数作用域，别全局污染

## 三、实际应用与最佳实践

### 3.1 场景：超时与等待 API

`sleep_for(100ms)`、`future.wait_for(2s)`、锁的 `try_lock_for(50ms)`——
所有 chrono 接口直接吃字面量，第 06 课的读写锁示例已经在这么用了。

### 3.2 场景：把 chrono 模式抄到自己的领域

单位进类型的思路完全可复制：`2_km + 500_m`、`4_GB`、`¥19.9`——
示例 ③-1 用一个 `Meters` 类型演示全套（constexpr 字面量算子 + 折算）。

### 3.3 最佳实践清单

1. 一切时间量写带后缀字面量，禁止裸数字
2. 转粗单位必须显式 `duration_cast`——让截断在代码里可见
3. `using namespace std::chrono_literals;` 放在函数作用域
4. 计时用 `steady_clock`（单调），别用 `system_clock`（会被校时干扰）
5. 自定义领域的单位类型，抄 chrono 作业：数值 + 编译期单位 + 混算规则

## 常见陷阱

1. `duration_cast` 向零截断：`90min` 转 `hours` 得 **1**（不是 1.5 也不是 2）；
   负值 `-1.5s` 转秒得 `-1`——C++17 才补了 `round/floor/ceil`（第 48 课有续篇）
2. `2h + 30min` 的类型是 `minutes`——拿 `hours` 变量接会编译错，
   这是保护不是麻烦；真要粗单位请显式 cast
3. 自定义字面量算子忘了下划线：`operator""km` 里 `km` 是保留标识符，
   编译错误；标准后缀也不能重定义
4. `0.5h == 30min` 为 true 但比较过程中类型已宽化到 long double rep——
   性能敏感路径别随手用浮点字面量
5. `count()` 只是裸数字，打印时单位信息丢失——面向用户的输出要自己补单位文字
6. **UDL 与整个字面量 token 绑定**（示例编译现场实录）：`100ms.count()` 会去找
   `operator""ms.count`，直接编译错误——先加括号再取成员：`(100ms).count()`
7. 整数字面量匹配 `unsigned long long` 重载、浮点匹配 `long double` 重载：
   自定义字面量算子只写 `long double` 版，`2_km` 这样的整数形式会编译失败
   （同样是示例实录）；另外 `operator"" _km` 中间的空格写法已废弃，
   新代码一律 `operator""_km`

## 完整示例

见 `08_chrono_literals.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++14 -Wall -Wextra "CPP14/08_chrono用户自定义字面量与时间单位/08_chrono_literals.cpp" -o run && ./run
```

## 练习

1. 写 `operator"" _KB / _MB / _GB` 返回 `Bytes` 类型（底层 `uint64_t` 字节数），
   验证 `2_GB == 2048_MB`。
2. 写一个重试循环：每 `500ms` 检查一次 `std::atomic<bool>` 就绪标志，
   最多等 `3s`——所有时间量只用字面量表达。
3. 用第 03 课的 `type_name` 技巧打印 `decltype(1.5h)` 的完整模板类型。
4. 对比 `duration_cast<hours>(25h).count()` 与 `25h / 1h` 的结果，
   解释为什么除法天生没有截断问题（提示：商是无单位的计数）。

## 参考

- cppreference: [chrono 字面量](https://zh.cppreference.com/w/cpp/chrono/literals)
- cppreference: [用户自定义字面量](https://zh.cppreference.com/w/cpp/language/user_literal)
- Howard Hinnant 的 chrono 论文/博客（chrono 设计者本人）
