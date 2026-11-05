# 第 17 课 · std::variant 与 std::visit（C++17）

> 所需标准：**C++17** ｜ 讲解：本文件 ｜ 独立代码：`17_variant_visit.cpp`

## 一、底层原理

### 1.1 带安全标签的 union

C 的 `union` 有两大原罪：**标签靠人维护**（记错了就是 UB）、**不能装
非平凡类型**（string/vector 有析构函数，union 管不了）。`std::variant<Ts...>`
是编译器代管的 tagged union：

| | C union + 手写 tag | 继承多态 | std::variant |
|---|---|---|---|
| 标签/类型识别 | 手维护，记错即 UB | RTTI/dynamic_cast | 编译器维护，`index()`/`holds_alternative` |
| 存储 | 就地 | **堆 + 指针** | **就地（栈上，sizeof≈最大成员）** |
| 值语义 | 无 | 无（引用语义） | 可拷贝/比较/放容器 |
| 类型集合 | 开放（谁都能塞） | 开放 | **封闭（列出候选）** |
| 成员要求 | 平凡类型 | — | 可有构造/析构，不可是引用/数组 |

"封闭集合 + 值语义 + 就地存储"三个性质合起来，就是函数式语言的
**和类型（sum type）**——C++17 终于把它标准化。

### 1.2 std::visit：variant 的模式匹配

`visit(f, v)` 在编译期检查访问器对各备选的调用是否合法，在运行期根据当前下标传入活动值。
C++17 要求各可能调用具有相同的返回类型和值类别。隐式转换和泛型兜底也可以满足可调用性要求。
单个 variant 的分派时间不随备选数增长，具体分派结构由实现决定。
如果 variant 处于 `valueless_by_exception()` 状态，访问会抛出 `std::bad_variant_access`。
参见 [C++17 草案 N4659 的访问规则](https://timsong-cpp.github.io/cppwp/n4659/variant.visit)。

### 1.3 overloaded 惯用法：三课知识的合体

```cpp
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;   // CTAD 指引
```

两行汇集了三个 C++17 特性：`using...` 包引入（第 14 课家族）、CTAD 显式
指引（第 15 课）、再配一串 lambda 就是编译期分派——variant 世界的标准姿势。

## 二、具体语法

```cpp
std::variant<int, std::string> v = 42;        // 默认构造第一个备选
v = "text"s;                                  // 赋值即切换备选（旧值析构）
std::holds_alternative<std::string>(v);       // 是 string 吗
std::get<std::string>(v);                     // 类型取值：不是则抛 bad_variant_access
std::get<0>(v);                               // 下标取值
auto p = std::get_if<int>(&v);                // 不匹配得 nullptr（不抛）
v.index();                                    // 当前备选下标
std::visit(f, v);                             // 分派
```

规则细则：

- 默认构造 = 第一个备选的默认构造（第一个备选必须可默认构造）
- 赋值切换时**旧成员先析构**再构造新成员——对象生命周期编译器全管
- 备选类型可以重复（`variant<int, int>`），但此时 `get<int>` 歧义，
  只能用下标
- 不允许引用、数组、void 备选

## 三、实际应用与最佳实践

### 3.1 场景：事件/消息系统（示例 ③-1）

一组封闭的消息类型（点击/按键/退出）打包成 `variant<Event...>` 入队，
消费端一个 `visit(overloaded{...})` 穷尽处理——比继承 + virtual 简洁，
比 switch(tag) 安全（编译器盯着穷尽性）。

### 3.2 场景：轻量状态机（示例 ③-2）

状态集合封闭、转换逻辑集中在 visit——状态本身携带各自数据，
比 enum + 散落字段清晰得多。

### 3.3 最佳实践清单

1. "封闭的类型集合，运行期二选一/多选一"用 variant，别用 union/继承硬凑
2. 处理 variant 一律 `visit` + overloaded；`get<T>` 只在"我确信是它"时用
3. 备选尽量不同类型；同类型不同语义用强类型包装（第 10 课的思路）
4. variant + optional 组合表达"可能没有 / 可能是 A 或 B"（第 19 课展开）
5. visit 的 lambda 用 `auto&&` 形参接值；需要只读就 `const auto&`

## 常见陷阱

1. **`get<T>` 类型不对抛 `bad_variant_access`**——不是 UB 但会 terminate
   （不接的话）；不确定就用 `get_if`
2. **默认构造是第一个备选**：`variant<A, B> v;` 里已经是 A 了——
   `index()==0`，别以为"空的"（variant 没有 optional 那种空态，
   `valueless_by_exception` 只是异常逃逸时的罕见残留态）
3. visit 的可调用物**必须覆盖全部备选**——漏一个编译错误；
   用泛型 lambda + if constexpr（第 13 课）兜底默认分支是常见解法
4. 备选类型相同（`variant<int, int>`）时 `get<int>` 编译歧义——
   换强类型包装或用下标
5. variant 成员切换 = 析构 + 构造：若成员构造抛异常，可能落入
   `valueless_by_exception` 态——查 `v.valueless_by_exception()`，
   但正常代码几乎遇不到

## 完整示例

见 `17_variant_visit.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++17 -Wall -Wextra "CPP17/17_std_variant与std_visit/17_variant_visit.cpp" -o run && ./run
```

## 练习

1. 给示例的事件系统加 `Scroll{int dy}` 事件，overloaded 里加对应分支，
   体会"漏分支编译不过"的穷尽性保证（先故意不加试试）。
2. 写 `variant<vector<int>, std::string>` 的 `total_size`：vector 走 size()，
   string 走 size()，用泛型 lambda 一行搞定；再想想什么时候必须分开写。
3. 用 `get_if` 重写 ①-3 的安全取值，对比三种取值方式的适用场景。
4. 把第 06 课 ConfigCache 的值类型改成 `variant<int, std::string, bool>`，
   写 `render(value)` 返回带类型标签的字符串。

## 参考

- 提案 N4218 / P0088: variant 系列
- cppreference: [std::variant](https://zh.cppreference.com/w/cpp/utility/variant)
- cppreference: [std::visit](https://zh.cppreference.com/w/cpp/utility/variant/visit)

## 学习衔接

继续阅读 [补充 17A · 多 variant 访问与状态转移](../补充知识点/17_补充A_多variant访问与状态转移/17_补充A_多variant访问与状态转移.md)。
