# 第 50 课 · std::expected：错误处理新范式（C++23）

> 所需标准：**C++23**（`-std=c++23`）｜ 独立代码：`50_expected.cpp`
> 本机示例使用 print，因而还需 -lstdc++exp；expected 本身不要求该链接库。
> 前置：第 16 课（optional）；第 19 课（类型与语义的取舍）

## 一、底层原理

### 1.1 用类型表示“值或错误”

`std::expected<T,E>` 成功时保存 T，失败时保存 E；`expected<void,E>` 表示
无返回数据的成功或错误。它与 optional 的区别是失败侧包含原因。

| 方式 | 失败表达 | 检查责任 | 信息与成本 |
|---|---|---|---|
| 异常 | throw | 可在合适层捕获；未捕获会终止 | 类型化信息，成本依 ABI、实现及失败频率而定 |
| 错误码 | 返回状态，可配 out 参数 | 调用者检查 | 状态值本身也可携带领域信息 |
| optional | nullopt | 调用者检查 | 不保存错误原因 |
| expected | unexpected / unexpect | 调用者检查或组合传播 | 值语义，大小与传递成本取决于 T/E |

expected 不会强迫调用者检查，也没有“忽略就必定报错”的语言机制。
为返回结果的函数显式加 `[[nodiscard]]` 可请求编译器诊断，但警告也不等于
强制处理。标准并不保证 expected 类自身具有 nodiscard 属性。

### 1.2 错误作为返回值，不等于整个函数不抛异常

expected 可在普通返回路径传播可预期失败，不必为这一失败执行栈展开。
但 T/E 的构造、复制、移动、分配以及回调仍可能抛异常；调用 value() 取错误
状态也会抛 bad_expected_access<E>。是否 noexcept 必须看整个实现。

本课用 from_chars 严格解析十进制端口 **1..65535**：接受前导零，不接受
正负号、空白或尾随文本（负号单独分类为 negative）。不要用未经检查的 stoi
假装实现相同契约：stoi 可接受前缀，还可能抛 invalid_argument/out_of_range。

### 1.3 单子式组合与恢复

```text
parse(s)                  expected<int,Err>
  .and_then(validate)     成功时调用返回 expected 的函数；错误原样传播
  .transform(convert)     成功时变换值；错误不变
  .or_else(recover)       失败时调用恢复/转换函数；也可返回成功值
  .value_or(fallback)     末端取值或丢弃原因使用默认值
```

and_then 的结果必须仍是 expected 且错误类型相同；or_else 的结果也必须是
expected 且成功值类型相同。transform_error 可变换错误类型。or_else 恢复成功
后，后续成功侧操作会继续，因此“错误后永远跳过所有后续操作”不是通用规则。

### 1.4 与 optional/异常的分工

需要让调用者按原因分支、失败属于常见流程时可用 expected；只关心有无时
optional 已足够。异常可把失败传播到合适的处理层。不能只按“是否可预期”
机械分类：API 约定、失败频率、异常策略、恢复点及 ABI 都会影响选择。
前置条件违约也不自动等于应当抛异常，须遵守相应接口契约。

## 二、具体语法

```cpp
#include <charconv>
#include <expected>
#include <string_view>

enum class Err { bad, range };
[[nodiscard]] std::expected<int,Err> parse_port(std::string_view s) {
    if (s.empty() || s.front() < '0' || s.front() > '9')
        return std::unexpected(Err::bad);
    int n = 0;
    auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), n);
    if (end != s.data() + s.size()) return std::unexpected(Err::bad);
    if (ec == std::errc::result_out_of_range || n < 1 || n > 65535)
        return std::unexpected(Err::range);
    if (ec != std::errc{}) return std::unexpected(Err::bad);
    return n;
}

auto r = parse_port("42");
if (r) { /* *r 或 r.value() */ } else { /* r.error() */ }
auto out = parse_port("7").and_then([](int n) -> std::expected<int,Err> {
    if (n <= 10) return std::unexpected(Err::range);
    return n * 2;
});
```

不能用 `condition ? int_value : std::unexpected(...)` 代替这里的 if/return：
这两个操作数不能因为函数返回 expected 就自动成为同一种条件表达式类型。

规则细则：

- `unexpected(e)` 构造错误包装；返回类型为 expected 时可在 return 处转换。
  也可 `expected<T,E>{std::unexpect, error_args...}` 原地构造错误侧。
- `*r` / `r->` 要求有值，error() 要求无值，违反前提在 C++23 中是 UB；
  value() 在无值时抛异常，且有相应错误类型可复制等要求。
- T 通常为对象类型或 void；E 须符合非数组、非 cv 对象等要求，不能是 void
  或引用。“任意 E”不包括所有 C++ 类型。
- 默认构造在 T 可默认构造时是成功状态；`expected<void,E>{}` 也是成功。
- E 为 enum、拥有型错误结构或 string 都可，按错误信息与成本选择。
  string_view 等借用型错误必须保证其数据寿命，不能引用局部错误文本。

## 三、实际应用与最佳实践

### 3.1 场景：解析与校验流水线

示例先解析，再变换、校验范围、记录错误。格式不合法、尾随字符、数字过大和
零端口都通过错误值返回，而不是漏到未说明的异常路径。格式化日志本身仍可抛异常。

### 3.2 场景：替代状态码与输出参数

expected<Port,ParseError> 把成功值和失败原因绑定在同一结果中；调用者通过
检查或组合选择处理方式。为公共结果函数加 nodiscard，不能依赖类型系统代查。

### 3.3 最佳实践清单

1. 明确输入语法、有效范围和失败分类，避免返回类型与实际行为不一致。
2. and_then 用于会失败的后续步骤，transform 用于值变换，or_else 用于错误侧。
3. 末端按需检查或 value_or；默认值参数本身会先求值，昂贵兜底可显式分支。
4. 错误类型要有适当所有权；大对象影响布局和搬运，不能只凭类型名推断成本。
5. expected 只描述已建模的返回失败，另外说明函数可能抛出的异常。

## 常见陷阱

1. 有值时 error()、无值时解引用，都会违反访问前提。
2. `return {"x"}` 可能编译失败，也可能构造成功值，取决于 T；它不表示错误。
   构造错误请明确写 unexpected 或 unexpect。
3. and_then 返回裸值或不同 E 不合法；只想变值用 transform。
4. 使用 string 错误并不意味着每次必然深拷贝：右值可移动，但布局和分配仍有成本。
5. 忽略 expected 完全可能；nodiscard 是接口的额外诊断约定。
6. stoi 返回值未经完整消费检查，不等于严格解析整个字符串。

## 完整示例

```bash
g++ -std=c++23 -Wall -Wextra "CPP23/50_std_expected/50_expected.cpp" -o run -lstdc++exp && ./run
```

## 练习

1. 用 from_chars 写 parse_list，定义空段、尾随字符和溢出的错误规则。
2. 分别在 expected<int,string> 与 expected<string,string> 函数中尝试
   `return {"x"}`，比较不合法与成功值构造，再改为 unexpected。
3. 写一个 or_else 恢复成功的链，验证后续 transform 会执行。
4. 比较大小不同的错误类型时，记录 sizeof、复制/移动次数与实际耗时。

## 参考

- [C++23 草案：expected 的类型与状态](https://timsong-cpp.github.io/cppwp/n4950/expected)
- [C++23 草案：访问前提与 value 的异常](https://timsong-cpp.github.io/cppwp/n4950/expected.object.obs)
- [C++23 草案：单子操作的类型要求](https://timsong-cpp.github.io/cppwp/n4950/expected.object.monadic)
- 提案 P0323R12（expected）/ P2505R5（单子操作）
