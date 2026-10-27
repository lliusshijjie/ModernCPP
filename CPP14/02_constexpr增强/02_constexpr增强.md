# 第 02 课 · constexpr 增强：编译期计算的解放（C++14）

> 所需标准：C++14 ｜ 讲解：本文件 ｜ 独立代码：`02_constexpr.cpp`

## 一、底层原理

### 1.1 constexpr 函数不是"编译期函数"，而是"可被编译期求值的普通函数"

最容易误解的一点，用一张表说清：

| 声明 | 保证 |
|---|---|
| `constexpr` 函数 | **可能**在编译期执行，**可能**运行期执行——取决于实参与结果用途 |
| `constexpr` 变量 | **必然**编译期初始化（初始化器必须是常量表达式） |
| `const` 变量 | 只是"初始化后不可修改"，可以用运行期值初始化 |

触发**编译期**求值的四类上下文（结果被用在"必须编译期算出"的地方）：

1. `constexpr` 变量的初始化器
2. 数组维度 / `template <int N>` 等模板实参
3. `static_assert` 的条件
4. 枚举值

```cpp
static_assert(factorial(5) == 120, "");  // 编译期执行
int x = factorial(5);                    // 标准不要求编译期执行（普通调用）
```

### 1.2 为什么 C++11 限制那么死——编译器是个"解释器"

编译期求值的本质：**编译器在编译过程中解释执行函数体**。C++11 的这个"解释器"
实现得极其保守，函数体内只允许：

| C++11 允许 | C++14 新允许 |
|---|---|
| 单条 `return` | 多条 return、if、switch |
| 无局部变量 | 局部变量（必须初始化） |
| 无任何修改 | 修改**本次求值中创建**的对象 |
| 无循环 → 迭代只能靠递归 | for / while / do / range-for |

所以 C++11 时代的编译期计算清一色是模板递归/递归 constexpr 的函数式风格；
C++14 把解释器升级为支持完整控制流（循环、局部栈、对象生命周期），
表达能力从"纯函数式"跳到"普通 C++"。这不是语法糖，是求值引擎的换代。

### 1.3 一份函数体，两个世界

编译器为同一个 constexpr 函数准备两条路径：

- **常量表达式上下文** → 解释执行，结果直接成为编译产物的一部分（立即数、表数据）
- **普通调用** → 生成常规机器码（`constexpr` 同时隐式 `inline`，方便头文件定义）

这解释了所有行为：实参带 `volatile`/运行期值 → 走运行期；结果进 `static_assert`
→ 编译期必须算出来，算不出就是编译错误。

### 1.4 隐藏变化：constexpr 成员函数不再隐式 const

C++11 规定 constexpr 成员函数必须是 `const`（因为它不许改任何状态）；
C++14 允许修改"求值中创建的对象"，隐式 const 随之移除。
从 C++11 迁移代码时注意：非 const 的 constexpr 成员函数从"非法"变"合法"。

## 二、具体语法

### 2.1 C++14 在 constexpr 函数体内新允许的

```cpp
constexpr int factorial(int n) {
    int result = 1;                  // 局部变量（必须初始化）
    for (int i = 2; i <= n; ++i) {   // 循环
        result *= i;                 // 修改局部变量
    }
    return result;                   // 单条 return 的限制也没了
}
```

### 2.2 仍然禁止的（C++14）

- `static` / `thread_local` 局部变量（编译期求值没有"全局存储"的概念）
- `goto`、`try-catch`
- 调用非 constexpr 函数；虚函数不能标 constexpr
- 未初始化的局部变量（求值拒绝未定义状态）

### 2.3 规则细则

- 函数是否标 constexpr 与"何时求值"是两回事（见 1.1 表）
- 传运行期实参给 constexpr 函数：合法，默默按普通函数执行，**不报错**
- `constexpr` 构造函数让类型成为"字面类型"，可以在编译期创建对象
  （示例的 `FactorialTable`）；`std::array` 的 **const** `operator[]` 在 C++14
  constexpr 化，但**可写**访问要等 C++17（见下方 3.1 的边界说明）

## 三、实际应用与最佳实践

### 3.1 场景：编译期生成查找表

CRC 表、三角函数表、阶乘表……C++11 要写模板递归或用外部脚本生成；
C++14 一个带循环的 constexpr 函数搞定，数据直接嵌进可执行文件，运行期零初始化成本。

**一个真实的边界**（示例编译时真踩到了）：语言上 C++14 允许修改"本次求值中
创建的对象"，但 `std::array` 的非 const `operator[]` 直到 **C++17** 才标记
constexpr——所以 C++14 编译期建表**写不进** `std::array`。解法是用内建数组的
字面类型包装，自己提供 constexpr 读/写访问：

```cpp
struct FactorialTable {
    int v[11] = {};                                        // 0! .. 10!
    constexpr int& at(int i) { return v[i]; }              // 可写访问
    constexpr int operator[](int i) const { return v[i]; } // 只读访问
};

constexpr FactorialTable make_factorial_table() {
    FactorialTable t;
    t.at(0) = 1;
    for (int i = 1; i < 11; ++i) t.at(i) = t.at(i - 1) * i;
    return t;
}
```

顺带演示了两个特性：constexpr 默认构造（NSDMI 让隐式构造函数 constexpr 化）、
constexpr 成员函数的读/写两个版本。C++17 起可直接用 `std::array`（后续课程见）。

### 3.2 场景：一份代码，两个世界

标了 constexpr 的纯函数，既服务编译期（建表、`static_assert` 单位换算校验），
也服务运行期（用户输入、随机数据）。后续标准持续给标准库既有函数补标
constexpr 走的正是这条路——先把自家纯函数标上，将来免费受益。

### 3.3 最佳实践清单

1. **想强制编译期求值**：结果接 constexpr 变量 / `static_assert` / 模板实参，三选一
2. **纯函数尽量标 constexpr**：写一次，编译期/运行期两用，无成本
3. **用 `static_assert` 当 constexpr 函数的单元测试**，错误在编译期暴露
4. **别为 constexpr 而 constexpr**：有 I/O、锁、堆分配的逻辑留在运行期
5. **调试提示**：编译期求值打不了断点——保留运行期调用路径便于单步跟踪

## 常见陷阱

1. **以为标了 constexpr 就一定编译期算**——传运行期实参它就默默走运行期，
   既不报错也不优化成编译期
2. 局部变量不初始化 → 编译错误（`int x;` 不行，`int x = 0;` 可以）
3. 在 constexpr 里用 static 局部变量/单例/懒加载 → C++14 不允许
4. 读普通全局变量的当前值 → 编译期求值看不到"运行期的它"，编译错误
5. C++11 → C++14 迁移：constexpr 成员函数不再隐式 const，
   影响重载决议和"能否修改成员"的判断
6. **语言允许 ≠ 标准库允许**：`std::array` 可写 `operator[]` 的 constexpr 到
   C++17 才有。库的 constexpr 化进度跟着标准版本走，别想当然

## 完整示例

见 `02_constexpr.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++14 -Wall -Wextra "CPP14/02_constexpr增强/02_constexpr.cpp" -o run && ./run
```

## 练习

1. 分别用 C++11 风格（递归）和 C++14 风格（循环）实现 `constexpr gcd(int, int)`，
   用 `static_assert` 对拍几组值，比较两版可读性。
2. 编译期生成前 20 个斐波那契数的查找表，`static_assert` 抽查
   `f(1)==1, f(10)==55, f(19)==4181`。（提示：为什么不能直接用
   `std::array` 的可写访问？参考示例的包装做法。）
3. 写 `constexpr bool is_palindrome(const char*)`（循环版），
   `static_assert` 验证 `"level"` 是回文、`"hello"` 不是。
4. 把阶乘表扩到 12 项（注意：`13!` 溢出 `int`——让编译器在编译期替你抓住这个 bug：
   写成 13 项试试，看 `static_assert` 或 constexpr 求值如何报错）。

## 参考

- cppreference: [constexpr 说明符](https://zh.cppreference.com/w/cpp/language/constexpr)
- 《Effective Modern C++》Item 15：尽可能使用 constexpr
