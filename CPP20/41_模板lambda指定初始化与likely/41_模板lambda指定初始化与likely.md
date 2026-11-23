# 第 41 课 · 模板 lambda、指定初始化器与 [[likely]]（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`41_lambda_designated_likely.cpp`
> 前置：第 01 课（泛型 lambda——本课补上"显式模板形参"这半边）；
> 第 31 课（concepts——约束模板 lambda）；第 02A（聚合初始化）

## 一、底层原理

### 1.1 模板 lambda：直接命名模板参数与参数关系

C++14 泛型 lambda 用 `auto` 形参引入隐式模板参数，但仍能通过
`decltype`、类型 traits、`auto&` 等保留并获取类型/数组信息；
C++17 起也能在泛型 lambda 内用 `if constexpr` 按类型分支。
C++20 的增量是**显式模板形参表**，可直接命名 T、N 并表达参数间的关系：

```cpp
auto len = []<class T, std::size_t N>(T (&arr)[N]) { return N; };
//                        ↑ 数组引用不退化，长度 N 直接到手
```

泛型 lambda 的闭包类有模板化的 operator()；C++20 的显式模板形参表
让类型/非类型参数、默认模板实参和约束直接可见。C++14 已支持
`auto...` 形参包，C++20 则可进一步显式命名 `class... Ts`。

### 1.2 指定初始化器：`.field = value`

```cpp
struct Window { int x = 0; int y = 0; std::string title = "untitled"; };
Window w{.y = 30, .title = "editor"};    // x 走默认值；跳着写、指名道姓
```

C++ 从 C99 借来指定初始化器，但**收窄成最保守的子集**：
指定式只适用于**聚合类类型**（不是直接初始化数组）、指示符必须**按声明顺序**、不能与位置初始化混用、
没有 C99 的数组指示符（如 `[0] = 1`）或嵌套指示符（如 `.a[0] = 1`）。
这样可按名字表达意图，并可跳过有合适默认值的字段；新增必需字段、
引用成员或改动默认值仍可能使已有初始化失效或改变行为。

### 1.3 [[likely]] / [[unlikely]]：给分支预测递话

`[[likely]]` / `[[unlikely]]` 向编译器表达路径的相对可能性，
可影响代码布局、内联等优化；它们不直接控制 CPU 的动态分支预测器。
属性不改变可观察行为，也不保证性能收益；是否使用应由测量决定。

## 二、具体语法

```cpp
[]<class T, std::size_t N>(T (&arr)[N]) { return N; }   // 数组长度
[]<std::integral T>(T v) { ... }        // 模板形参加 concept 约束
[]<class... Ts>(Ts&&... vs) { ... }     // 变参包 lambda
auto f = []<class T>(const std::vector<T>& v) { return v.front(); };

Window w{.y = 30, .title = "editor"};   // 按声明序、可跳过
Theme t{.fg = "white", .login{.x = 1, .y = 2}};   // 嵌套指定
// Window bad{.title = "x", .y = 1};   // 编译错：乱序
// Window bad2{.x = 1, 30};            // 编译错：指定与位置混用
// int a[2] 内的 .a[0]=1               // 编译错：无数组指示符

if (ok) [[likely]] { fast_path(); } else [[unlikely]] { slow_path(); }
for (;;) [[likely]] { ... }             // 也可标注循环体
```

规则细则：

- 指定初始化器四条边界（全部实测编译错）：**必须聚合**（有用户声明
  或继承构造函数即出局）、**必须按声明序**、**不能与位置混用**、**无数组指示符**。
  聚合类还须满足无私有/保护直接数据成员、无虚函数等其他聚合规则
- 未指定的非 union 成员先用默认成员初值，否则用空初始化列表初始化；
  引用成员若既无默认成员初值也未被指定，会导致程序不合法
- 属性可用于**语句或标签**；标记分支体可写 `if (ok) [[likely]] { ... }`，
  标记标签则写 `[[likely]] case 1:`，不是写在条件表达式里
- 模板 lambda 的模板实参可显式给：`f.operator()<int>(...)`；
  通常仍靠推导
- lambda 捕获与模板形参独立：`[cap]<class T>(T x) {...}` 合法

## 三、实际应用与最佳实践

### 3.1 场景：配置对象的"自解释构造"（示例 ①）

十几二十个字段的配置结构，位置初始化 `{0, 30, "editor", false}` 是
猜谜；`.title = "editor"` 一眼可读——review 时少吵一半的架。

### 3.2 场景：不退化的数组接口（示例 ②）

模板 lambda / 模板函数以 `T(&)[N]` 收数组：长度进类型系统，
与 `std::span`（第 37 课）构成"取长 + 借用"组合拳。

### 3.3 最佳实践清单

1. 多字段聚合初始化一律指定式——配合默认成员初值，
   "只写关心的字段"
2. 需要类型/长度元信息的 lambda 就上模板形参表；
   配合 concept 约束（`[]<std::integral T>`）报错也友好
3. `[[likely]]/[[unlikely]]` 只在**测出来**的热路径用；
   凭感觉标注是在误导优化器
4. 指定初始化器与构造函数二选一：类型一旦有构造逻辑，
   指定式就退役（改用具名工厂函数）
5. 只标有证据的路径即可，无需成对标记；避免给多条竞争路径标上
   互相矛盾的概率提示

## 常见陷阱

1. **乱序指定**编译错——C++ 不学 C99 的任意序；
   初始化顺序仍按成员声明顺序；不要仅为迁就初始化列表而随意改成员布局
2. **指定 + 位置混用**编译错：`{.a = 1, 2}` 非法
   （C99 允许"指定后续跟位置"，C++ 明确禁止）
3. 聚合里**数组成员**只能整块指定或跳过，`.a[0] = 1` 写法不存在
4. 类型后来加了构造函数 → 所有指定初始化器**一夜编译错**
   ——这是聚合边界的信号，不是"语法坏了"
5. `[[likely]] if (...)` 语法上可用，但属性作用于整个 if 语句，
   不能据此表达条件为 true 的分支更常见；应标记相应分支体
6. 参数写成 `T arr[N]` 不是按值复制数组：函数形参中的数组类型会调整为
   `T*`，N 无法据此推导。保留数组类型和长度要用 `T(&)[N]`

## 完整示例

见 `41_lambda_designated_likely.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra "CPP20/41_模板lambda指定初始化与likely/41_lambda_designated_likely.cpp" -o run && ./run
```

## 练习

1. 把一个 6 字段配置结构分别用位置初始化与指定初始化写一遍，
   故意交换两个字段类型兼容的值，观察指定式如何帮助人工审查；
   字段类型兼容时，编译器不保证能发现填错值。
2. 写模板 lambda `sum_all`：变参包 `Ts...` + 折叠表达式（第 14 课），
   要求只接受算术类型（concept 约束）。
3. 用 `T(&)[N]` 模板 lambda 实现 `avg_of_array`，配合 `std::span`
   写一个重载——两版对 C 数组各返回什么？
4. 写一个带 `[[likely]]`/`[[unlikely]]` 的解析函数（输入合法走快路径），
   用 `-O2` 对比有无标注的运行时间、汇编布局或硬件计数器（可选：`__builtin_expect` 思想）。

## 参考

- [C++20 草案：聚合与指定初始化](https://timsong-cpp.github.io/cppwp/n4861/dcl.init.aggr)
- [C++20 草案：likely/unlikely 属性](https://timsong-cpp.github.io/cppwp/n4861/dcl.attr.likelihood)
- 提案 P0428R2（模板 lambda）/ P0329R4（指定初始化器）/
  P0479R5（likely/unlikely）
- cppreference: [指定初始化器](https://zh.cppreference.com/w/cpp/language/initialization#指定初始化器)
- cppreference: [属性 likely](https://zh.cppreference.com/w/cpp/language/attributes/likely)

## 补充阅读

[41A · 结构化类类型作为非类型模板参数](../补充知识点/41_补充A_结构化类类型模板参数/41_补充A_结构化类类型模板参数.md)。从模板语法继续到类类型模板实参，用固定字符串标记字段。
