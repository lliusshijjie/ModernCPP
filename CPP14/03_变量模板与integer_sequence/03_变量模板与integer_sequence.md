# 第 03 课 · 变量模板与 std::integer_sequence（C++14）

> 所需标准：C++14 ｜ 讲解：本文件 ｜ 独立代码：`03_variable_template.cpp`

## 一、底层原理

### 1.1 变量模板：C++11 模板家族缺的那一块

C++11 里函数、类、类型别名都能模板化，唯独**变量**不行。于是所有
"随类型变化的编译期值"只能寄生在类模板的静态成员里——这就是 traits 的来历：

| | C++11 写法 | C++14 写法 |
|---|---|---|
| 定义 | `template<class T> struct X { static constexpr T v = ...; };` | `template<class T> constexpr T x = ...;` |
| 使用 | `X<double>::v` | `x<double>` |
| 每类型一个谓词 | traits 结构 + 继承 true_type/false_type | 变量模板 + 偏特化 |

**编译器视角**：变量模板是一个"按模板参数实例化的**变量家族**"。
`pi_t<float>`、`pi_t<double>` 是两个彼此独立的实体，实例化规则与类模板完全相同：
用到才实例化、支持全特化与偏特化。再与 constexpr 组合，每个实例都是编译期常量。

### 1.2 integer_sequence：数据编码在类型里的"空壳"

标准库里它的真实形态惊人地简单：

```cpp
template <class T, T... Is>
struct integer_sequence {};      // 空结构体！
```

| 组成 | 存放在哪 |
|---|---|
| 索引数据 `0,1,2` | **模板参数包**（类型的一部分） |
| 对象本身 | 空壳，`sizeof == 1`（1000 个索引也一样大） |
| 何时"取出" | 函数形参推导出 `Is...`，再包展开 |

这不是浪费，恰恰是精髓：数据在**类型**里，编译器才能在编译期把它完全展开成
无循环的直线代码——手工循环展开的机器化版本。它存在的唯一目的是
**把参数包从类型世界搬运到表达式世界**。

### 1.3 它解决 C++11 的什么痛点：包展开的"最后一公里"

C++11 已有可变参数模板，但"把 tuple 的每个元素依次喂给表达式"只能写递归
重载层层剥皮，样板代码不堪入目。integer_sequence 的分工：

- `make_index_sequence<N>` 负责批量生成索引 `0..N-1`（编译期）
- 包展开 `Is...` 负责一次性铺开成表达式序列

两者合起来相当于把"编译期 for 循环"标准化了。C++14 没有结构化绑定和折叠
表达式，这套手法就是当年展开 tuple 的唯一正道（C++17 的两个新语法会取代
其中一半，见第 09、10 课）。

## 二、具体语法

### 2.1 变量模板

```cpp
template <class T>
constexpr T pi_t = T(3.141592653589793238462643L);

template <> constexpr int pi_t<int> = 3;              // 全特化

template <class T>  constexpr bool is_ptr_v     = false;
template <class T>  constexpr bool is_ptr_v<T*> = true;   // 偏特化：当"类型谓词"
```

规则细则：

- 使用形式 `名字<模板实参>`，与函数模板加括号、类模板加尖括号一样自然
- 用到哪个实例才实例化哪个
- 静态数据成员同样可以做成模板：`struct X { template<class T> static constexpr int v = 1; };`
- `_v` 后缀惯用法：把 traits 的 `::value` 拖尾样板消灭掉
  （C++17 标准库正式收录了 `is_same_v` 等一整套）

### 2.2 integer_sequence 家族

```cpp
std::integer_sequence<T, Is...>          // 原始形态：T 是元素类型
std::make_integer_sequence<T, N>         // 生成 0..N-1
std::index_sequence<I...>                // 元素类型固定 size_t 的别名
std::make_index_sequence<N>              // 最常用
std::index_sequence_for<Ts...>           // = make_index_sequence<sizeof...(Ts)>
```

取用三件套：函数形参**按值**接住空壳 → 推导出 `Is...` → `sizeof...(Is)` 数个数、
`Is...` 展开使用。

### 2.3 展开的万能落点（C++14 版）

没有折叠表达式的年代，把包铺开成表达式靠**数组初始化列表**：

```cpp
const T arr[] = { Is... };                          // 数据型展开
int dummy[] = { 0, (副作用表达式, 0)... }; (void)dummy;   // 副作用型展开（逗号表达式）
```

C++17 的折叠表达式（第 10 课）会让这两行退休，但现在它们是标准手法。
注意空包展开进数组是非法的（数组至少一个元素），空序列要特判。

## 三、实际应用与最佳实践

### 3.1 场景：展开 tuple —— index_sequence 的头号用法

打印 tuple、把 tuple 解包成函数实参（自制 `apply`），全部依赖
`index_sequence_for<Ts...>` 生成索引再展开 `std::get<Is>(t)...`。
示例 ③-1、③-2 是可直接抄走的两个模板。

### 3.2 场景：类型级常量与 _v 惯用法

- 物理引擎/数值代码：`kEpsilon<T>`、`pi_t<T>` —— 一个模板，float/double/long double 各得其所
- 元编程风格代码：`is_ptr_v<T>` 这类偏特化变量模板当类型谓词用，比 traits 短一半

### 3.3 最佳实践清单

1. C++14 里"每类型一个常量"一律用 constexpr 变量模板，别再写 traits 结构
2. 自定义谓词按 `_v` 后缀命名，向 C++17 标准库风格对齐，将来平滑迁移
3. index_sequence 形参**按值**传递——空壳零成本，别写成 const 引用装深沉
4. C++14 展开用数组初始化 + 逗号表达式；升级 C++17 后换成折叠表达式
5. `make_index_sequence<N>` 的实例化成本随 N 增长，N 控制在千级以内

## 常见陷阱

1. **每个实例是独立实体**：非 constexpr 的变量模板里放可变状态，
   `v<int>` 和 `v<double>` 的状态互不相干——你改的这个不是你想的那个
2. `is_ptr_v<const int*>` 匹配 `T = const int`（指向 const 的指针），
   而 `int* const`（const 指针）**不**匹配 `T*`——偏特化模式匹配的层次要想清楚
3. 空 pack 展开进 `T arr[] = { Is... };` 是编译错误，空序列需要单独重载
4. `integer_sequence` 元素必须是整型（常用 `size_t`），浮点、指针都不行
5. `make_index_sequence` 大 N 会拖慢编译、错误信息变长——需要大表时考虑
   编译期分治生成或构建期脚本

## 完整示例

见 `03_variable_template.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织
（其中用 GCC/Clang 专有的 `__PRETTY_FUNCTION__` 亲眼看整数序列的类型）：

```bash
g++ -std=c++14 -Wall -Wextra "CPP14/03_变量模板与integer_sequence/03_variable_template.cpp" -o run && ./run
```

## 练习

1. 写变量模板 `is_integral_v<T>`（基于 `std::is_integral`），并用
   `static_assert` 验证 `int` 为真、`double` 为假。
2. 用 index_sequence 写 `sum_tuple(const std::tuple<Ts...>&)`，返回所有元素之和，
   用 `make_tuple(1, 2.5, 3)` 验证得 6.5。
3. 用 `make_index_sequence<10>` 生成 0..9 的平方表，`static_assert` 抽查
   第 3、7 项是 9、49。
4. 把示例的 `print_tuple` 升级为带索引前缀：`0:1, 1:2.5, 2:cpp`。

## 参考

- cppreference: [变量模板](https://zh.cppreference.com/w/cpp/language/variable_template)
- cppreference: [std::integer_sequence](https://zh.cppreference.com/w/cpp/utility/integer_sequence)
