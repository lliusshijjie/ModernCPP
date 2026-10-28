# 补充 03A · 类型萃取与 SFINAE 约束（C++14）

> 阅读顺序：第 03 课之后、第 04 课之前；不占每日主线课编号。
> 所需标准：C++14 ｜ 独立代码：`03a_type_traits_sfinae.cpp`

## 一、底层原理

### 1.1 从“计算类型”到“选择接口”

第 03 课介绍了把类型映射为常量的变量模板；类型萃取还可以把类型映射为另一个类型。
C++14 为一批具有 `::type` 的标准库工具提供 `_t` 别名，减少模板声明中的样板：

```cpp
template <class T>
using normalized_t = std::decay_t<T>;
// 等价于 typename std::decay<T>::type
```

类型萃取、别名模板和 SFINAE 都不是 C++14 才有的机制。
本课新增工具是 C++14 的 `_t` 别名，并用它们组织此前缺少的泛型接口案例。
并非所有 trait 都有标准 `_t` 别名；例如不要据此编造 `std::is_integral_t`。

### 1.2 SFINAE：替换失败让候选退出

编译器给函数模板替换模板实参时，如果直接上下文中的类型或表达式不合法，
对应候选可以退出重载集合。这是 SFINAE：Substitution Failure Is Not An Error。

```cpp
template <class T>
std::enable_if_t<std::is_integral<T>::value, const char*>
number_kind(T) { return "integral"; }
```

`enable_if_t<true, R>` 是 `R`；条件为 `false` 时没有可用的 `type`。
因此这个候选只接受满足条件的 `T`。候选退出之后如果没有其他可用重载，
整个调用仍然编译失败。

### 1.3 函数体错误不会自动变成 SFINAE

把限制放在函数体的 `static_assert` 中，可以改善报错，但不会提前排除候选。
同样，`auto` 返回类型依赖函数体实例化时，函数体中的错误通常也是硬错误。
需要参与重载筛选的条件应放在函数声明的直接上下文中，具体边界见补充 07A。

## 二、具体语法

### 2.1 常用类型变换

| 工具 | 作用 | 注意 |
|---|---|---|
| `std::remove_reference_t<T>` | 去掉引用 | 保留被引用类型的 `const` |
| `std::remove_cv_t<T>` | 去掉顶层 cv | 不会递归去掉指针所指类型的 `const` |
| `std::decay_t<T>` | 模拟按值传参的类型变换 | 还会把数组、函数退化为指针 |
| `std::conditional_t<B, T, F>` | 在两个类型之间选择 | 两个类型实参自身都必须能形成 |
| `std::enable_if_t<B, R>` | 条件成立时提供类型 | 常用作候选的开关 |

`std::decay_t<T>` 不是“取得所有权”：数组退化得到的指针不会延长数组的生命周期。
只想去掉引用和顶层 cv 时，可以组合 `remove_cv_t<remove_reference_t<T>>`。

### 2.2 检测表达式是否存在

配套代码定义一个本地 `void_t14`，用 `decltype(std::declval<const T&>().size())`
检测 `const T&` 上的 `size()` 表达式。表达式存在就选择偏特化，不存在则保留主模板。

这里的 `void_t14` 是教学辅助别名；标准库的 `std::void_t` 要到 C++17 才提供。
`declval` 只在不求值上下文中使用，不需要真实构造一个 `T`。

### 2.3 类型查询还可以作为对象传递

`std::true_type`、`std::false_type` 是 `integral_constant` 的别名。
C++14 新增其调用运算符，因此 `std::integral_constant<int, 3>{}()` 得到 `3`。
配套代码也展示了 C++14 的 `std::is_final<T>::value`。

这些工具既可以参与重载，也可以作为标签参数。C++14 可以自行定义 `_v` 变量模板，
但不能使用 C++17 才有的标准 `std::is_integral_v` 等名称。

## 三、实际应用与最佳实践

### 3.1 场景：给整数与浮点数选择不同实现

使用两个互斥的 `enable_if_t` 条件，分别筛选整数和浮点数重载。
相比运行期 `if`，不适用的重载不会进入最终候选；相比仅在函数体报错，
其他合法重载仍有机会被选择。

注意 `std::is_integral<bool>::value` 也为真。如果业务不接受布尔值，
应显式增加排除条件，不能把语言分类当成业务规则。

### 3.2 场景：为提供 size() 的对象开放接口

`has_size<T>` 可以作为 `length_of` 的开关，但它只证明目标表达式存在。
示例的接口还要求结果能转换为 `std::size_t`；如果需要支持任意第三方类型，
应进一步检测返回类型，而不能把“存在一个名字”当成完整契约。

## 常见陷阱

1. 把 `static_assert`、SFINAE、运行期条件分支当成相同机制。
2. 对引用直接使用 `is_integral<T>`，却忘记 `int&` 不是整数类型。
3. 使用 `decay_t` 后忘记数组已经变成指针，长度信息已经丢失。
4. 以为 `conditional_t` 可以隐藏另一个类型实参中的任意非法表达式。
5. 两个重载的条件有交集，导致二义性；应先设计互斥的接口规则。

## 完整示例

从 `ModernCPP` 目录执行：

```bash
g++ -std=c++14 -Wall -Wextra -Wpedantic "CPP14/补充知识点/03_补充A_类型萃取与SFINAE约束/03a_type_traits_sfinae.cpp" -o run
```

生成后在 PowerShell 执行 `./run.exe`，在 Linux/macOS 执行 `./run`。

## 练习

1. 写出 `const int&` 依次经过 `remove_reference_t`、`remove_cv_t` 后的类型。
2. 让 `number_kind` 的整数重载排除 `bool`，观察调用时的候选变化。
3. 把 `has_size` 改成检测非 const 对象，比较仅提供非 const `size()` 的类型。
4. 扩展检测：要求 `size()` 的返回类型可转换为 `std::size_t`。

## 参考

- WG21：[N3655 · TransformationTraits Redux](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3655.pdf)（第 4 部分未纳入 C++14）
- WG21：[N3638 · Return type deduction](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3638.html)（SFINAE 一节）
- WG21：[C++11 到 C++14 的主要变化](https://open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1319r0.html)

## 学习衔接

返回 [第 03 课](../../03_变量模板与integer_sequence/03_变量模板与integer_sequence.md)，
然后阅读 [第 04 课](../../04_make_unique与智能指针最佳实践/04_make_unique与智能指针最佳实践.md)。
学完第 07 课后，继续阅读
[补充 07A](../07_补充A_转发返回值与代理对象/07_补充A_转发返回值与代理对象.md)。
