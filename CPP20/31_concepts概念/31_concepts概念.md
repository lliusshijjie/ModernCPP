# 第 31 课 · Concepts 概念（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`31_concepts.cpp`
> 前置：第 13 课（if constexpr 替代"类型分派"——本课补上"接口约束"这另一半）

## 一、底层原理

### 1.1 旧世界的两副镣铐

模板想表达"T 必须能 X"，C++20 之前常用 SFINAE/`enable_if`/tag 派发，
也可在模板体内用 `static_assert` 检查，但后者不能据此排除重载候选：

| | enable_if / SFINAE | **concepts** |
|---|---|---|
| 写法 | 重载 + 条件类型，意图埋在返回类型里 | 声明处直接约束，**约束即文档** |
| 错误信息 | 千行模板展开，天文数字行号 | 通常能更直接指出不满足的约束（诊断长度由实现决定） |
| 约束组合 | `&&` 嵌套惨烈 | `&& \|\| !` 直接写 |
| 重载优选 | 手动 tag 派发分层 | **结构化约束包含关系**参与优选（subsumption） |

第 13 课的 if constexpr 干掉了"分派"的样板；concepts 干掉的是"约束"的样板
——两者经常配合使用；现有 SFINAE 代码仍然有效，也有各自适用的场景。

### 1.2 concept 的本质：命名的编译期谓词

```cpp
template <class T>
concept Addable = requires(T a, T b) { a + b; };   // 类型 T 的"能力清单"
```

**编译器视角**：concept 是命名的编译期约束；`Addable<T>` 这样的
concept-id 是 `bool` 类型的表达式，但 concept 并不是变量模板。重载决议会
归一化约束，按原子约束的同一性判断 **subsumption**，不会证明任意逻辑蕴含。
例如 `Numeric = std::integral<T> || std::floating_point<T>` 复用了标准概念，
`std::integral<T>` 才能在相应重载间构成更强约束；独立重写相似条件未必可以。

### 1.3 requires 表达式四件套

```cpp
template <class T>
concept Container = requires(T c, const T cc) {
    typename T::value_type;                       // ① 类型要求：嵌套类型存在
    { c.size() } -> std::convertible_to<std::size_t>;   // ② 复合要求：表达式合法+返回可转
    { cc.empty() } -> std::same_as<bool>;          //    （还能加 noexcept）
    requires std::is_default_constructible_v<T>;   // ③ 嵌套要求：再嵌一个约束
    // ④ 简单要求：a + b; —— 表达式合法即可，不求值
};
```

## 二、具体语法

### 2.1 三种使用姿势（声明接口时选择合适形式）

```cpp
template <Addable T>                     // 姿势一：直接当"类型"用
T add1(T a, T b) { return a + b; }

template <class T> requires Numeric<T>   // 姿势二：requires 子句（可写复杂组合）
T twice(T x) { return x + x; }

void print_num(Numeric auto x) { ... }   // 姿势三：auto 简写（最轻量）
```

这里的 Addable 仅检查 `a + b` 合法；若算法还要求结果能转回 T，
应增加 `{ a + b } -> std::convertible_to<T>`，或让函数返回类型自动推导。

### 2.2 标准库 <concepts> 常用件

`same_as`、`derived_from`、`convertible_to`、`integral/floating_point/signed_integral`、
`equality_comparable/strict_weak_order`、`default_initializable/move_constructible`、
`invocable/regular_invocable`、`swappable`。
`std::ranges::range` 属于 `<ranges>`，不在 `<concepts>` 中（第 32 课）。

### 2.3 规则细则

- 概念绝不能被偏特化/重定义（与第 03 课变量模板的关键差异）
- 可用 `&&`/`||` 组合约束；否定通常加括号，如 `requires (!std::integral<T>)`。
  `!C<T>` 整体作为原子约束，归一化不会替你做布尔代数化简
- 约束包含（subsumption）参与重载优选；约束不可比较时仍可能歧义，
  类型转换和模板偏序等规则也仍然适用
- `requires requires` 双写是真的（前子句后表达式），不是笔误
- 简写 `Numeric auto` 可与返回 `auto` 组合，形参约束照样参与决议

## 三、实际应用与最佳实践

### 3.1 场景：给算法上"接口说明书"（示例 ③-1）

`sum(a, b, c)` 约束 `Numeric`——传 string 时没有满足约束的候选。
错误通常能定位到具体约束，实际诊断格式和长度取决于编译器。

### 3.2 场景：重载分层自动化（示例 ③-2）

同一 `pick`：无约束版兜底、`integral` 版定制——调用处按类型自动落位，
这是适合用约束重载取代 tag 派发的一种场景。

### 3.3 最佳实践清单

1. 按算法真正依赖的能力约束公共泛型 API；无需为每个模板参数强加无关约束
2. 优先组合标准库 concepts，自定义 concept 用 requires 表达式说"能力"
   而非罗列类型名单
3. 有明确约束层级时复用命名 concept，让 subsumption 参与优选
4. `auto` 形参与 concept 组合是简洁派首选；复杂约束用 requires 子句
5. concept 命名用能力形容词（Addable/Container），不用类型名单
   （Int32OrDouble——那是枚举不是约束）

## 常见陷阱

1. **requires 表达式不求值**：`requires { f(); }` 只检查"能调用"，
   f 不会执行（副作用、断言都不会跑）
2. `{ expr } -> std::same_as<X>` 检查的是 **`decltype((expr))`**，包含
   cv/ref 限定；例如左值成员表达式可能得到 `int&`，并不等于 `int`。
   `convertible_to` 检查可转换性，并另有语义要求
3. 约束写得太窄（只许 int）退化成模板装饰；写得太宽（只要求可拷贝）
   等于没写——按"算法真正用到的能力"下刀
4. 同一翻译单元重定义 concept 会报错；不同翻译单元定义不一致违反 ODR，
   不保证编译器或链接器诊断。用头文件保护统一定义
5. 同一模板的重声明要保持约束的语法形式与顺序；不要把
   `template <Addable T>` 改写成 `template <class T> requires Addable<T>`
   来重声明。逻辑等价不保证声明等价，某些违规情况不要求诊断

## 完整示例

见 `31_concepts.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra "CPP20/31_concepts概念/31_concepts.cpp" -o run && ./run
```

## 练习

1. 写 `Serializable` concept：要求 `to_string(T)` 合法且返回可转 `string`，
   给 int/double/custom 三种类型验证。可先在 concept 所在命名空间写
   `using std::to_string;`，再用非限定调用，让标准重载和自定义类型的 ADL 都能参与。
2. 把第 25 课的 `transform_reduce` 包装加约束：op 可调用、init 可转换。
3. 用 subsumption 写三层层级（无约束 → Numeric → integral），分别用
   double/int/struct 调用，验证自动落位。
4. 故意把 string 传给示例的 `sum`，把编译器的错误信息抄进注释——
   体会与 enable_if 时代的差距。

## 参考

- [C++20 草案：约束归一化与包含关系](https://timsong-cpp.github.io/cppwp/n4861/temp.constr.order)
- [C++20 草案：受约束声明的重声明规则](https://timsong-cpp.github.io/cppwp/n4861/temp.constr.decl)
- 提案 P0734R0（concepts 入 C++20；源自 Concepts Lite 十年长跑）
- cppreference: [约束（concepts）](https://zh.cppreference.com/w/cpp/language/constraints)
- cppreference: [标准库 concepts](https://zh.cppreference.com/w/cpp/concepts)
