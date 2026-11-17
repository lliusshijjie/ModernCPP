# 第 44 课 · constexpr string/vector：编译期容器（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`44_constexpr_containers.cpp`
> 前置：第 02 课（constexpr 增强）；第 40 课（consteval/constinit——本课是其容器篇续集）

## 一、底层原理

### 1.1 补齐最后一块：constexpr 的动态内存

C++17 的 constexpr 能算递归、能分支（第 13 课），但**不能分配内存**——
`std::vector` / `std::string` 整个被挡在编译期门外（g++ 报
"不是常量表达式"）。C++20 三连击：

- **P0784**：`new`/`delete`/分配器进常量求值
- **P0980**：`basic_string` 的 constexpr 化
- **P1004**：`vector` 的 constexpr 化

从此编译期能用"会长的容器"写复杂算法，不必再硬凑 `std::array`
的定长世界。

### 1.2 瞬态分配铁律：草稿纸可以撕，但不能带出考场

常量求值期间的堆分配必须**全部在求值结束前释放**——分配不能
"活着逃出去"（non-transient allocation）。推论三条：

1. `constexpr` 函数内建 vector、push_back、sort，函数返回前销毁——合法
2. 想把结果**存下来**，必须先拷出到**字面类型**（`std::array`、
   整数、`string_view`…）——"拷出模式"是本课的核心套路
3. `constexpr std::vector<int> v{1,2,3};` 作为**变量**直接编译错
   （g++ 实测：refers to a result of 'operator new'；
   类型上 vector 也非字面类型）

### 1.3 双模红利：编译期校验，运行期复用

同一个 `constexpr` 函数：编译期调用（`static_assert`）是**免跑的
单元测试**；运行期调用照常执行（实参不必是常量）。
"逻辑只写一份、两个世界共享"是 C++20 容器 constexpr 化的最大工程红利。

## 二、具体语法

```cpp
constexpr std::array<int, 6> sorted_squares() {
    std::vector<int> v{5, 1, 4, 1, 3};       // 编译期建容器
    for (auto& x : v) x = x * x;             // constexpr 修改
    std::sort(v.begin(), v.end());           // constexpr 算法
    std::array<int, 6> r{};                  // 拷出到字面类型
    for (std::size_t i = 0; i < v.size(); ++i) r[i] = v[i];
    return r;                                // v 销毁：瞬态合规
}
static_assert(sorted_squares()[4] == 25);    // 编译期"单测"
constexpr auto t = sorted_squares();         // 常量表：进只读数据

// constexpr string：拼接/下标修改/比较
constexpr std::size_t greet_len() {
    std::string a = "Hello";
    a += ", world";
    a[0] = 'h';
    return a.size();
}

// constexpr auto bad = new std::vector<int>{1};     // 编译错：分配逃逸
// constexpr std::vector<int> v{1, 2, 3};            // 编译错：非字面类型
```

规则细则：

- 拷出目标必须是**字面类型**：`std::array` / 算术 / 枚举 / 引用 /
  只含字面成员的聚合；`vector`/`string` 本体不行
- 常量求值里的**未定义行为**（越界、漏析构）会被编译器当场拒绝——
  这反而是查 bug 利器
- `constexpr` 成员函数不一定"必须在编译期跑"（那是 consteval，
  第 40 课）；容器成员如 `push_back` 标了 constexpr，运行期照用
- lambda 立即调用（IIFE）是造 constexpr 表的惯用外壳：`constexpr auto t = []{...}();`
- 常量求值深度/步数有实现限制（gcc `-fconstexpr-ops-limit`）——
  编译期计算量要克制

## 三、实际应用与最佳实践

### 3.1 场景：编译期查表（示例 ③）

CRC 表、单位换算表、游戏数值表——vector 草稿算完拷进 `constexpr array`：
零启动开销、进只读段（还能被分页共享）。C++17 做同样的事只能手写
定长数组或模板递归。

### 3.2 场景：编译期单测（示例 ②/③）

`static_assert(f(args) == expected)` 一行一个断言，编译一次全验——
纯函数逻辑（解析、变换、归约）先在编译期钉死，运行期测试只留
I/O 与状态。

### 3.3 最佳实践清单

1. 容器在 constexpr 里定位是**草稿纸**：算完拷出、用完即毁
2. 表格数据一律"constexpr 函数 + 拷出 array"模式，别写死手维护的字面量
3. 纯逻辑函数尽量 constexpr 化——`static_assert` 白拿一层测试
4. 结果要动态大小？拷出改为**返回给运行期**（函数不留 constexpr 语境
   调用即可），编译期只测正确性
5. 编译时间也是成本：大体量递归/循环先量一量再上（慢编译的 constexpr
   是团队之耻）

## 常见陷阱

1. **`constexpr std::vector` 变量**编译错——不是实现不完整，是
   "瞬态分配"规则的本意：常量里不允许住着堆内存
2. 分配**逃出**常量求值（返回 new 出的指针、静态成员里藏 vector）
   → 编译错："refers to a result of operator new"
3. 拷出时**大小对不上**：目标 array 6 格、源 vector 5 个元素，尾巴留 0
   ——示例 ① 的输出尾部就是 0，预测输出时最容易栽在这里
4. constexpr 函数里调用**未 constexpr 化**的接口（如 `<cstdio>`、
   `getenv`）→ 该函数永远无法在编译期求值——runtime 路径另拆
5. 以为编译器**一定会**在编译期跑：constexpr 只是"可以"；
   要强制用 consteval（第 40 课）或 static_assert 钉住
6. 函数指针参数收 lambda（示例 2.3 的谓词）注意**返回类型精确匹配**
   （int 返回的 lambda 转 `bool(*)(int)` 编译错）

## 完整示例

见 `44_constexpr_containers.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra "CPP20/44_constexpr编译期容器/44_constexpr_containers.cpp" -o run && ./run
```

## 练习

1. 写 `constexpr std::array<int, 10> primes10()`：vector 试除法筛出
   前 10 个素数后拷出；用 `static_assert` 验证第 10 个是 29。
2. 写 `constexpr bool is_palindrome(std::string_view s)`（string_view
   是字面类型可直接进 constexpr），对 "level"/"lever" 各断言一次。
3. 故意写越界（`v[v.size()]`）放进 constexpr 函数并 static_assert
   调用——观察编译器如何把运行期 UB 变成编译期拒绝。
4. 把你项目里一张手写的常量表换成"constexpr 函数生成"，对比
   维护性与二进制大小。

## 参考

- 提案 P0784R5（常量求值中的 new/分配器）/ P0980R1（constexpr string）/
  P1004R1（constexpr vector）
- cppreference: [常量表达式（constexpr 的容器支持）](https://zh.cppreference.com/w/cpp/language/constant_expression)
- cppreference: [std::vector 的 constexpr 说明](https://zh.cppreference.com/w/cpp/container/vector)
