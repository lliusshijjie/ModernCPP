# 第 50 课 · std::expected：错误处理新范式（C++23）

> 所需标准：**C++23**（`-std=c++23`；本机同前课需 `-lstdc++exp` 链接旗子——
> CPP23 篇示例统一带上）｜ 讲解：本文件 ｜ 独立代码：`50_expected.cpp`
> 前置：第 16 课（optional——"可能没有值"的单形态前辈）；第 19 课（三者取舍）

## 一、底层原理

### 1.1 第四条错误处理之路

C++ 此前三条路各有残缺：

| 方式 | 失败表达 | 调用方强制检查 | 携带错误信息 | 成本 |
|---|---|---|---|---|
| 异常 | throw | 否（不接就穿透） | 任意类型 | 零成本模型 + 栈展开贵 |
| 错误码/enum | 返回值 | **否**（忘查=静默错） | 需另开 out 参 | 低 |
| optional | nullopt | **弱**（得主动查） | **无**（只有"没有"） | 低 |
| **expected<T,E>** | unexpected | **类型系统逼你拆包** | **任意 E** | 低（值语义） |

`std::expected<T, E>`（P0323，源自 Rust 的 `Result` / Hazard 模型）：
**要么是 T，要么是 E，二选一且必须显式取出**——
optional 的"有/无"升级为"值/错误原因"。

### 1.2 值语义的胜利：错误路径不上栈

异常的失败传播靠栈展开（且不可预测何时抛）；expected 把错误当
**返回值**搬运：拷贝/移动即传递，`[[nodiscard]]` 加持下忘记处理
会得到编译警告。整条错误链没有隐藏控制流——**读代码就是读数据流**。

### 1.3 单子式组合：错误自动短路

```
parse(s)            expected<int, Err>
  .and_then(validate)   值 → 继续（返回 expected）；错误 → 原样透传
  .transform(double)    只变换成功值，错误不动
  .or_else(log_it)      只处理错误侧
  .value_or(0)          拆包兜底
```

链上任何一环出错，后续 `and_then/transform` 全部跳过，
错误直通末端——**没有一层层 if 也没有 try 块**（示例 ②）。

### 1.4 与 optional/异常的分工

- **可预期的失败**（解析失败、未找到、校验不过）→ expected：
  它是流程的一部分，不是"异常状况"
- **不可预期/不变量破坏**（内存耗尽、前置条件违约）→ 异常：
  expected 强迫每层都接反而污染
- "只关心有没有、没有原因" → optional 仍然够用（第 19 课结论不变）

## 二、具体语法

```cpp
#include <expected>

std::expected<int, std::string> parse_pos(std::string_view s) {
    if (s.empty())  return std::unexpected("empty");
    if (s[0] == '-') return std::unexpected("negative");
    return std::stoi(std::string(s));
}

auto r = parse_pos("42");
r.has_value();  r.value();        // true / 42（无值时 value() 抛
                                  // bad_expected_access<E>）
r.error();                        // 错误侧（有值时 UB 级契约违约）
r.value_or(-1);                   // 拆包兜底
*r;                               // 有值侧解引用（同 optional）

// 单子组合（错误自动短路）：
auto out = parse_pos("7")
    .transform([](int x) { return x * 2; })
    .and_then([](int x) -> std::expected<int, std::string> {
        return x > 10 ? x : std::unexpected("too small");
    });

std::expected<void, E> f();       // 只要"成功/失败+原因"时 T 用 void
```

规则细则：

- `unexpected<E>{e}` 是**构造错误侧**的辅助类型（不能直接当返回值用，
  要转成 expected——隐式转换在 return 处发生）
- `error()` 在**有值**时调用是未定义契约（libstdc++ 可能断言）——
  先 `has_value()` 分流
- `operator*`/`operator->` 访问值侧，不检查（同 optional 的
  "信自己"通道）
- expected 支持比较（值侧相等 / 错误侧相等）、可 `[[nodiscard]]`
- `transform_error` 变换错误类型；链式换错误类型要显式对齐
- E 建议**轻量**（enum / 错误码 / string_view）：expected 靠值
  返回，E 又大又贵就失去意义

## 三、实际应用与最佳实践

### 3.1 场景：解析与校验流水线（示例 ②）

读配置 → 校验范围 → 换算单位，三步都可能失败：
`and_then` 串起来错误自动短路，**任何一步失败都直达终点错误侧**——
与嵌套 if / 层层 try 对比，代码形状是线性的。

### 3.2 场景：替代"错误码 + out 参"的 C 风格接口（示例 ③）

`bool get_port(int& out)` 的现代形态：
`expected<Port, ParseError> get_port()`——
返回值即全部信息，`[[nodiscard]]` 下忘查编译器直接警告。

### 3.3 最佳实践清单

1. 可预期失败一律 `[[nodiscard]] auto f() -> expected<T, Err>`
2. 错误类型定成**项目统一 enum/struct**（别随手 string）——
   顶层还能 `switch` 分发
3. 链式优先 `and_then/transform`，别拆包再装包；
   末端才 `value_or` / 检查
4. 与异常划清边界：库的可预期失败给 expected；
   "真异常"（bad_alloc 级）继续让异常穿透
5. 性能敏感处避免 expected<pair 大结构>——错误侧与值侧
   共享存储布局，值类型过大整体变胖（union 语义）

## 常见陷阱

1. **有值时调 error()**：契约违约（UB/断言）——error() 只在
   `!has_value()` 的分支里用
2. `return std::unexpected(...)` 写成 `return {..}`：
   花括号直构的是**值侧**——错误悄悄变成了"值"
3. and_then 的可调用**必须返回 expected**（返回裸值编译错）；
   只想变值用 transform——两者别混
4. `*r` 不检查：同 optional，无值解引用是 UB——不确定时 value()
   （抛异常）或先 has_value()
5. 忘 `[[nodiscard]]`：错误被静默吞掉，expected 的最大保险失效
6. E 选 std::string 又到处拷：错误路径每次传值都深拷贝——
   轻量 E 或引用语义（错误码 + 全局错误表）

## 完整示例

见 `50_expected.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++23 -Wall -Wextra "CPP23/50_std_expected/50_expected.cpp" -o run -lstdc++exp && ./run
```

## 练习

1. 写 `expected<vector<int>, Err> parse_list(string_view "1,2,3")`：
   逐段 stoi，任何一段失败整链失败（and_then/transform 组合）。
2. 复现陷阱 2：把 `return std::unexpected("x")` 误写成
   `return {"x"}`，观察类型系统如何反应。
3. 把第 16 课练习里的 optional 接口升级为 expected（带上失败原因），
   调用方打印原因而非静默兜底。
4. 性能小实验：expected<double, BigErr{char[256]}> 与
   expected<double, ErrCode{enum}> 各在循环里传递百万次，
   对比耗时并解释。

## 参考

- 提案 P0323R12（std::expected）/ P2505R5（单子式操作）
- cppreference: [std::expected](https://zh.cppreference.com/w/cpp/utility/expected)
- cppreference: [std::unexpected](https://zh.cppreference.com/w/cpp/utility/expected/unexpected)
