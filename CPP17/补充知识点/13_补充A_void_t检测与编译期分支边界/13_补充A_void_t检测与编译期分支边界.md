# 补充 13A · void_t 检测与编译期分支边界（C++17）

> 阅读顺序：第 13 课之后、第 14 课之前；不占每日主线编号。
> 所需标准：C++17 ｜ 独立代码：`13a_detection_and_constraints.cpp`。
> 前置：C++14 补充 03A 的类型萃取和 SFINAE。

## 一、底层原理

### 1.1 先检测表达式，再选择实现

`if constexpr` 能根据编译期条件选择实现，但条件本身必须合法。
如果对象不一定有 `size()`，可以先用检测惯用法得到一个总能查询的布尔 trait。

```cpp
template <class T, class = void> struct has_size : std::false_type {};
template <class T>
struct has_size<T, std::void_t<decltype(std::declval<const T&>().size())>>
    : std::bool_constant<std::is_convertible_v<
          decltype(std::declval<const T&>().size()), std::size_t>> {};
```

当表达式不能形成时，偏特化不适用，主模板给出 false；能形成时，再验证返回类型是否
能隐式转换为 `size_t`。`declval` 只用于不求值上下文，不会真实创建或调用对象。

### 1.2 C++17 新增了哪些工具

| 工具 | 本节用途 | 与已有机制的关系 |
|---|---|---|
| `std::void_t<T...>` | 将可形成的类型列表映射为 void | 替代 C++14 补充课的本地 `void_t14` |
| `std::bool_constant<B>` | 依据条件生成 true/false trait | `integral_constant<bool, B>` 的别名 |
| `std::is_integral_v<T>` 等 `_v` | 查询 trait 的布尔值 | 对应已有的 `Trait<T>::value` |
| `std::conjunction` | 按需实例化多个 trait 的 `value` | 与普通表达式中的 `&&` 有不同的实例化边界 |

SFINAE、偏特化、`decltype`、`declval` 都不是 C++17 才有的机制。
同样，不应把 `_v` 后缀误认为能自动去掉引用：`is_integral_v<int&>` 仍为 false。

### 1.3 检测契约必须与实际调用一致

本例检查的是对 `const T&` 对象调用 `size()`。仅提供非 const `size()` 的 `MutableOnly` 不满足契约；
返回字符串的 `WrongResult` 也不满足数值转换要求。
检测成功不证明业务意义正确，例如返回的长度是否真实、是否合理，需要另一层约定。

## 二、具体语法

### 2.1 归一化引用和 cv

配套代码用 `remove_cv_t<remove_reference_t<T>>` 得到 `plain_t<T>`，
再查询 `is_integral_v<plain_t<T>>`。只移除 cv 与引用，不把数组自动退化成指针。
这是 C++14 工具与 C++17 `_v` 查询的衔接，不使用 C++20 的 `remove_cvref_t`。

### 2.2 函数体选择与候选筛选

`size_or_zero` 接受任意本例可用类型；有合规 `size()` 就调用，否则返回 0。
`required_size` 则用 `enable_if_t` 把接口限制在检测成功的类型上。
前者的 `if constexpr` 不会自动让函数退出重载集合，后者的 SFINAE 会参与候选筛选。

### 2.3 一个容易误用的逻辑条件

```cpp
// 不足以保护 T=int：条件表达式也需要形成合法的 T::value_type。
if constexpr (has_value_type<T>::value &&
              std::is_integral_v<typename T::value_type>) { /* ... */ }
```

普通逻辑短路不等于可以跳过右侧的任意类型检查。配套代码的 `classify` 分两层写：
先确认成员类型存在，再在被选中的分支里形成依赖该成员的第二个条件。

另一种写法是 `conjunction<has_value_type<T>, integral_element<T>>`：
第一个 trait 为 false 时，不要求实例化后一个 trait 的 `value`。
但后一个模板实参必须先能作为类型形成；把非法的 `typename T::missing` 直接写进模板实参，
仍然不能靠 `conjunction` 隐藏。

## 三、实际应用与最佳实践

### 3.1 场景：为提供长度的对象开放能力

日志统计、缓冲区包装等接口可以针对 const 对象是否提供可转换长度选择实现。
先准确描述需要的表达式，再决定“缺能力时回退”还是“拒绝成为候选”，
避免把返回 0 的宽接口误用在必须具有长度的场景。

### 3.2 分离存在性、类型和业务条件

存在性由检测 trait 负责，返回类型要求由转换或类型查询负责，
运行时长度是否超限由普通 `if` 负责。`if constexpr` 不检查数据的实时值。
对于不支持类型的最终报错，可沿用战斗练习中的依赖型 `static_assert`，
从而兼顾原始 C++17 工具链；不依赖后续缺陷报告对 `static_assert(false)` 的放宽。

## 常见陷阱

1. 只检测函数名字，却忘记其 const、引用限定和返回类型要求。
2. 对引用类型直接查询 `_v`，误判其被引用对象的分类。
3. 把 `if constexpr` 当成 SFINAE，期望它排除一个函数候选。
4. 用普通 `&&` 掩盖一个本来就非法的成员类型。
5. 以为 trait 能证明业务正确性或运行时数据的安全范围。

## 完整示例

从 `ModernCPP` 目录执行：

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic "CPP17/补充知识点/13_补充A_void_t检测与编译期分支边界/13a_detection_and_constraints.cpp" -o run
```

PowerShell 执行 `./run.exe`；Linux/macOS 执行 `./run`。
示例用静态断言验证不支持的类型，并在运行时输出宽接口与受约束接口的结果。

## 练习

1. 将检测对象从 `const T&` 改为 `T&`，预测 `MutableOnly` 的检测结果。
2. 取消 `required_size(42)` 的注释，和 `size_or_zero(42)` 比较编译行为。
3. 将 `classify` 的两层判断合并成上面的 `&&`，观察 `classify(42)` 的诊断。
4. 增加一个存在 `value_type`、但元素为浮点类型的容器，验证分类结果。

## 参考

- C++17 草案 N4659：[type_traits 概要](https://timsong-cpp.github.io/cppwp/n4659/meta.type.synop)。
- C++17 草案 N4659：[conjunction、disjunction 和 negation](https://timsong-cpp.github.io/cppwp/n4659/meta.logical)。
- C++17 草案 N4659：[if constexpr 的实例化规则](https://timsong-cpp.github.io/cppwp/n4659/stmt.if)。

## 学习衔接

前置参见 [C++14 补充 03A](../../../CPP14/补充知识点/03_补充A_类型萃取与SFINAE约束/03_补充A_类型萃取与SFINAE约束.md)。
返回 [第 13 课](../../13_if_constexpr编译期分支/13_if_constexpr编译期分支.md)，然后阅读
[第 14 课](../../14_折叠表达式/14_折叠表达式.md)。目录见 [补充知识点](../README.md)。
