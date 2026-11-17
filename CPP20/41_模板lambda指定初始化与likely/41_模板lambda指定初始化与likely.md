# 第 41 课 · 模板 lambda、指定初始化器与 [[likely]]（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`41_lambda_designated_likely.cpp`
> 前置：第 01 课（泛型 lambda——本课补上"显式模板形参"这半边）；
> 第 31 课（concepts——约束模板 lambda）；第 02A（聚合初始化）

## 一、底层原理

### 1.1 模板 lambda：补上 `auto` 推导不出的一块

C++14 泛型 lambda 的形参只能是 `auto`——**模板形参本身拿不到**：
数组的长度、容器的元素类型、按类型分派的 `if constexpr` 都做不了。
C++20 允许 lambda 带**显式模板形参表**：

```cpp
auto len = []<class T, std::size_t N>(T (&arr)[N]) { return N; };
//                        ↑ 数组引用不退化，长度 N 直接到手
```

编译器视角：lambda 的闭包类本来就有一个模板化的 `operator()`，
C++14 是"偷偷模板"；C++20 把模板形参表亮出来——
**约束（第 31 课）、默认模板实参、变参包**从此都能写。

### 1.2 指定初始化器：`.field = value`

```cpp
struct Window { int x = 0; int y = 0; std::string title = "untitled"; };
Window w{.y = 30, .title = "editor"};    // x 走默认值；跳着写、指名道姓
```

C++ 从 C99 借来指定初始化器，但**收窄成最保守的子集**：
只能用于**聚合类型**、指示符必须**按声明顺序**、不能与位置初始化混用、
没有数组指示符（`.a[0] = 1` 是 C99 的，C++ 没有）。
换来的是：**字段名即文档**、新增字段不破坏既有初始化、默认值显式可读。

### 1.3 [[likely]] / [[unlikely]]：给分支预测递话

优化器为每个分支预填流水线，猜错代价高昂。`[[likely]]` 标注"这个分支
大概率走"，让代码布局/预测器倾向于它——**提示**而已，语义零变化。
真实收益场景：错误分支 `[[unlikely]]`、快路径 `[[likely]]` 的热循环。

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
  构造函数即出局）、**必须按声明序**、**不能与位置混用**、**无数组指示符**
- 指示的字段可跳过未指示的；未指示字段用**默认成员初值**或值初始化
- `[[likely]]` 放在**语句**前（if/for/while 的分支体、case 标签后），
  不是放在条件表达式里
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
5. 分支属性成对写（`[[likely]]` 快路径 + `[[unlikely]]` 错误分支）
   比单写一个更能传达意图

## 常见陷阱

1. **乱序指定**编译错——C++ 不学 C99 的任意序；
   想换比较顺序改声明序（对照第 35 课 defaulted `<=>` 同理）
2. **指定 + 位置混用**编译错：`{.a = 1, 2}` 非法
   （C99 允许"指定后续跟位置"，C++ 明确禁止）
3. 聚合里**数组成员**只能整块指定或跳过，`.a[0] = 1` 写法不存在
4. 类型后来加了构造函数 → 所有指定初始化器**一夜编译错**
   ——这是聚合边界的信号，不是"语法坏了"
5. `[[likely]]` 写在 `if` 前面（`[[likely]] if (...)`）不是标分支——
   位置不同含义不同；标准位置是**分支语句**前
6. 模板 lambda 的数组形参必须**按引用** `T(&)[N]`——按值 `T arr[N]`
   早已被语言禁止

## 完整示例

见 `41_lambda_designated_likely.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra "CPP20/41_模板lambda指定初始化与likely/41_lambda_designated_likely.cpp" -o run && ./run
```

## 练习

1. 把一个 6 字段配置结构分别用位置初始化与指定初始化写一遍，
   故意交换两个字段类型兼容的值，观察哪种写法能当场暴露错误。
2. 写模板 lambda `sum_all`：变参包 `Ts...` + 折叠表达式（第 14 课），
   要求只接受算术类型（concept 约束）。
3. 用 `T(&)[N]` 模板 lambda 实现 `avg_of_array`，配合 `std::span`
   写一个重载——两版对 C 数组各返回什么？
4. 写一个带 `[[likely]]`/`[[unlikely]]` 的解析函数（输入合法走快路径），
   用 `-O2` 对比有无比标注的分支预测差异（可选：`__builtin_expect` 思想）。

## 参考

- 提案 P0428R2（模板 lambda）/ P0329R4（指定初始化器）/
  P0479R5（likely/unlikely）
- cppreference: [指定初始化器](https://zh.cppreference.com/w/cpp/language/initialization#指定初始化器)
- cppreference: [属性 likely](https://zh.cppreference.com/w/cpp/language/attributes/likely)
