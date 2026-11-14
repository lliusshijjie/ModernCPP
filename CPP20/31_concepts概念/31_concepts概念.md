# 第 31 课 · Concepts 概念（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`31_concepts.cpp`
> 前置：第 13 课（if constexpr 替代"类型分派"——本课补上"接口约束"这另一半）

## 一、底层原理

### 1.1 旧世界的两副镣铐

模板想表达"T 必须能 X"，C++20 之前只有 SFINAE/`enable_if`/tag 派发：

| | enable_if / SFINAE | **concepts** |
|---|---|---|
| 写法 | 重载 + 条件类型，意图埋在返回类型里 | 声明处直接约束，**约束即文档** |
| 错误信息 | 千行模板展开，天文数字行号 | 一句"不满足约束 XXX（因为……）" |
| 约束组合 | `&&` 嵌套惨烈 | `&& \|\| !` 直接写 |
| 重载优选 | 手动 tag 派发分层 | **包含关系自动优选**（subsumption） |

第 13 课的 if constexpr 干掉了"分派"的样板；concepts 干掉的是"约束"的样板
——两兄弟合体，SFINAE 从此只在最深的库里才有出场机会。

### 1.2 concept 的本质：命名的编译期谓词

```cpp
template <class T>
concept Addable = requires(T a, T b) { a + b; };   // 类型 T 的"能力清单"
```

**编译器视角**：concept 就是一个 `constexpr bool` 模板（对每个 T 求真/假），
但比裸 bool 多两件事：错误信息带名字和原因；重载决议能理解约束的
**包含关系**（`integral` 比 `Numeric` 严格 → 有 `integral` 版就选它）。

### 1.3 requires 表达式四件套

```cpp
template <class T>
concept Container = requires(T c, const T cc) {
    typename T::value_type;                       // ① 类型要求：嵌套类型存在
    { c.size() } -> std::convertible_to<size_t>;   // ② 复合要求：表达式合法+返回可转
    { cc.empty() } -> std::same_as<bool>;          //    （还能加 noexcept）
    requires std::is_default_constructible_v<T>;   // ③ 嵌套要求：再嵌一个约束
    // ④ 简单要求：a + b; —— 表达式合法即可，不求值
};
```

## 二、具体语法

### 2.1 三种使用姿势（约束力相同，位置不同）

```cpp
template <Addable T>                     // 姿势一：直接当"类型"用
T add1(T a, T b) { return a + b; }

template <class T> requires Numeric<T>   // 姿势二：requires 子句（可写复杂组合）
T twice(T x) { return x + x; }

void print_num(Numeric auto x) { ... }   // 姿势三：auto 简写（最轻量）
```

### 2.2 标准库 <concepts> 常用件

`same_as`、`derived_from`、`convertible_to`、`integral/floating_point/signed_integral`、
`equality_comparable/strict_weak_order`、`default_initializable/move_constructible`、
`invocable/regular_invocable`、`swappable/range`（配合 ranges，第 32 课）

### 2.3 规则细则

- 概念绝不能被偏特化/重定义（与第 03 课变量模板的关键差异）
- 约束表达式里 `&&`/`||`/`!` 自由组合：`Numeric = integral || floating_point`
- 同名重载间的优选按**约束包含**（subsumption）自动裁决——不需要 tag
- `requires requires` 双写是真的（前子句后表达式），不是笔误
- 简写 `Numeric auto` 可与返回 `auto` 组合，形参约束照样参与决议

## 三、实际应用与最佳实践

### 3.1 场景：给算法上"接口说明书"（示例 ③-1）

`sum(a, b, c)` 约束 `Numeric`——传 string 直接编译错，错误信息一行看懂；
对比旧世界 `enable_if` 版的报错（注释里存了原文）。

### 3.2 场景：重载分层自动化（示例 ③-2）

同一 `pick`：无约束版兜底、`integral` 版定制——调用处按类型自动落位，
tag 派发的完整替代。

### 3.3 最佳实践清单

1. 公共泛型 API 的**每个模板参数**都配约束——文档即代码，报错即教学
2. 优先组合标准库 concepts，自定义 concept 用 requires 表达式说"能力"
   而非罗列类型名单
3. 重载优选交给 subsumption；别再手写 tag 层级
4. `auto` 形参与 concept 组合是简洁派首选；复杂约束用 requires 子句
5. concept 命名用能力形容词（Addable/Container），不用类型名单
   （Int32OrDouble——那是枚举不是约束）

## 常见陷阱

1. **requires 表达式不求值**：`requires { f(); }` 只检查"能调用"，
   f 不会执行（副作用、断言都不会跑）
2. concept 里 `{ expr } -> same_as<X>` 的返回检查要求**精确匹配语义**——
   `convertible_to` 与 `same_as` 别混用（一个宽松一个严格）
3. 约束写得太窄（只许 int）退化成模板装饰；写得太宽（只要求可拷贝）
   等于没写——按"算法真正用到的能力"下刀
4. 同一 concept 名在两个头文件定义不同 → 编译错（概念不可重定义）
5. `template <Addable T>` 与 `template <class T> requires Addable<T>`
   混用于同名重载时约束等价即可声明相同——乱组合会出现歧义而非优选

## 完整示例

见 `31_concepts.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra "CPP20/31_concepts概念/31_concepts.cpp" -o run && ./run
```

## 练习

1. 写 `Serializable` concept：要求 `to_string(T)` 合法且返回可转 `string`，
   给 int/double/custom 三种类型验证。
2. 把第 25 课的 `transform_reduce` 包装加约束：op 可调用、init 可转换。
3. 用 subsumption 写三层层级（无约束 → Numeric → integral），分别用
   double/int/struct 调用，验证自动落位。
4. 故意把 string 传给示例的 `sum`，把编译器的错误信息抄进注释——
   体会与 enable_if 时代的差距。

## 参考

- 提案 P0734R0（concepts 入 C++20；源自 Concepts Lite 十年长跑）
- cppreference: [约束（concepts）](https://zh.cppreference.com/w/cpp/language/constraints)
- cppreference: [标准库 concepts](https://zh.cppreference.com/w/cpp/concepts)
