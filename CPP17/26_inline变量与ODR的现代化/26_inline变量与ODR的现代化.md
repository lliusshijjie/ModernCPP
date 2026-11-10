# 第 26 课 · inline 变量与 ODR 的现代化（C++17）

> 所需标准：**C++17** ｜ 讲解：本文件 ｜ 独立代码：`26_inline_variables.cpp` + 迷你头文件 `26_config.hpp`

## 一、底层原理

### 1.1 ODR：头文件里放不下变量的年代

**单一定义规则（ODR）**：每个变量/函数在整个程序里只能有一个定义。
头文件被多个 .cpp include 时，里面的 `int counter = 0;` 会在每个翻译单元
都生成一份定义 → **链接器重定义错误**。C++17 之前只有三条弯路：

| 需求 | C++14 的绕法 | 样板量 |
|---|---|---|
| 头文件全局常量 | `static constexpr` 成员 + **类外再定义一份**（被 odr-use 时） | 两处 |
| 头文件可变全局 | Meyers 函数（`static int& counter(){ static int c; return c; }`） | 一函数 |
| extern 声明 + .cpp 定义 | 头文件 `extern`，另建 .cpp 放真身 | 两个文件 |

### 1.2 inline 的真面目：不是"内联展开"，是"多重定义许可"

C++98 给**函数**的 `inline` 的核心语义从来不是内联提示，而是：
**允许多个翻译单元各有一份定义，链接器把它们合并为同一个实体**。
C++17 把这个语义扩展到**变量**——`inline int counter = 0;` 放头文件，
N 个 .cpp include，链接后仍然只有一个 `counter`。头文件库的全局状态
问题一刀解决。

### 1.3 连带修复：constexpr static 成员隐式 inline

C++17 起，类内 `static constexpr` 数据成员**隐式 inline**——
被 odr-use（取地址、绑引用）时**不再需要类外定义**。
C++14 里 `struct C { static constexpr int id = 42; };` 后 `const int& r = C::id;`
会直接链接报错（undefined reference），C++17 无痛通过。

### 1.4 一个仍要小心的细节：命名空间作用域的 constexpr 不隐式 inline

只有**类内 static constexpr 成员**隐式 inline；命名空间/头文件里的
`constexpr int x = 1;` 靠 const 的内部链接躲开 ODR——**每个 TU 各一份、
地址不同**。要"唯一实体"（传 `const T&`、比较地址）就显式写
`inline constexpr`。

## 二、具体语法

```cpp
// 某头文件（可被任意多个 .cpp include）
inline int api_calls = 0;                        // 可变全局：合并为一个实体
inline const std::string version = "1.2.3";      // 动态初始化的 inline 常量
inline constexpr int max_retry = 3;              // 常量：唯一实体 + 唯一地址

struct Config {
    static constexpr double ratio = 0.5;         // C++17：隐式 inline
};                                               // const double& r = Config::ratio; OK
```

规则细则：

- 各 TU 中的 inline 变量定义**必须完全一致**，否则 UB（通常无诊断）——
  定义只放头文件，绝不抄进 .cpp
- 整个程序**恰好初始化一次**
- 跨 TU 的**动态初始化顺序仍未指定**（static init order fiasco 没消失；
  有顺序依赖就用 Meyers 函数——局部 static 的首次初始化顺序是确定的）

## 三、实际应用与最佳实践

### 3.1 场景：header-only 库的全局状态（示例 ③-1）

版本字符串、全局计数器、默认参数——以前被迫拖一个 .cpp 或包一层函数，
现在头文件里一行 `inline`。示例的 `26_config.hpp` 就是可直接抄的模板。

### 3.2 场景：类内常量的 odr-use（示例 ①-2）

`static constexpr` 成员直接取引用/地址——C++14 时代的链接错误
成为历史。

### 3.3 最佳实践清单

1. 头文件可变全局 → `inline`；要唯一地址/引用的常量 → `inline constexpr`
2. 只在编译期用的常量 → 普通 `constexpr` 即可（每 TU 一份无所谓，
   还省了链接器合并）
3. 类内 `static constexpr` 直接用，删掉所有类外定义（C++17 起是死代码）
4. 有初始化顺序依赖的全局 → 仍用 Meyers 函数，别赌未指定顺序
5. inline 变量定义只出现在头文件——.cpp 里的"再写一遍"是 UB 陷阱

## 常见陷阱

1. **头文件裸写 `int x = 0;`**：每个 include 的 .cpp 一份定义 → 链接
   重定义错误；加 `static` 则每 TU 一份**状态分裂**（各改各的）——
   正解是 `inline`
2. 以为 `inline` 是内联优化提示——对变量毫无此意；对函数也只是
   "多重定义许可 + 一个弱化的优化建议"
3. 命名空间 `constexpr` 不隐式 inline：多个 TU 地址不同，
   `&x` 比较或 `const T&` 形参绑定可能踩坑
4. 跨 TU 动态初始化顺序未指定：A 头文件的 inline 变量初始化时
   用 B 头文件的 inline 变量——时序听天由命
5. inline 变量 + `static` 修饰混写：`static inline` 在命名空间作用域
   会退化成内部链接（每 TU 一份），语义被 static 覆盖

## 完整示例

见 `26_inline_variables.cpp` 与迷你头文件 `26_config.hpp`（模拟真实
header-only 库，cpp include 头文件后使用其中的 inline 变量）：

```bash
g++ -std=c++17 -Wall -Wextra "CPP17/26_inline变量与ODR的现代化/26_inline_variables.cpp" -o run && ./run
```

## 练习

1. 把 `26_config.hpp` 复制成 `extra.cpp` 也 include 的第二个 TU
   （`#include "26_config.hpp"` + 一个自增函数），编译
   `g++ -std=c++17 26_inline_variables.cpp extra.cpp -o run`，
   验证两处 `&api_calls` 相同——亲手证实"合并为一个实体"。
2. 去掉 `26_config.hpp` 里的 `inline` 再链接练习 1，观察重定义错误的
   原文（看完还原）。
3. 写 Meyers 单例 `Registry& registry()`，与 inline 变量版全局对比
   初始化顺序保证的差异。
4. 在头文件里定义 `constexpr int k = 1;` 与 `inline constexpr int kk = 1;`，
   用练习 1 的双 TU 分别打印 `&k` 与 `&kk`，观察前者不同、后者相同。

## 参考

- 提案 P0386R2（inline variables）
- cppreference: [inline 说明符](https://zh.cppreference.com/w/cpp/language/inline)
- cppreference: [单一定义规则](https://zh.cppreference.com/w/cpp/language/definition)
