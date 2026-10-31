# 第 13 课 · if constexpr：编译期分支（C++17）

> 所需标准：**C++17** ｜ 讲解：本文件 ｜ 独立代码：`13_if_constexpr.cpp`

## 一、底层原理

### 1.1 三种"条件"的根本区别

| | 普通 `if` | `if constexpr` | 特化 / SFINAE |
|---|---|---|---|
| 判定时机 | 运行期 | **模板实例化期** | 模板实例化期 |
| 未选分支 | 完整编译 | 模板内**被丢弃，不实例化** | 只编译匹配的那份 |
| 解决什么 | 值不同走不同路 | **类型不同走不同路** | 同上（C++17 之前的苦路） |

`if constexpr` 的条件必须是常量表达式——**编译期就知道走哪边**，
另一边在模板实例化时被整体丢弃：里面的依赖代码不实例化、不检查。
这一个性质终结了 SFINAE 套娃、tag dispatch、特化递归终点三座大山。

### 1.2 "丢弃"的精确边界（重要的细节）

- **模板内**：未选分支不实例化——依赖当前类型参数的构造不会出错
- **模板外**：两个分支都会被完整语义检查（没有模板可延迟）——
  这时 if constexpr 意义不大，别为风格而风格
- 无论哪边：语法/词法错误照报；非依赖构造仍会被检查

### 1.3 旗舰案例：std::advance 的三代实现

| 时代 | 写法 |
|---|---|
| C++98 | 按迭代器类别分派 **3 个重载**（input/random/bidirectional 各一份） |
| C++11 | enable_if/SFINAE 选择，一样绕 |
| C++17 | **一个函数体**，`if constexpr` 按 `iterator_category` 走 `+=` 或循环 |

## 二、具体语法

```cpp
if constexpr (cond) {...} else {...}          // cond 必须是常量表达式
if constexpr (init; cond) {...}               // 可与第 12 课的 init 语句叠加
if constexpr (A) {...}
else if constexpr (B) {...}                   // else if constexpr 链 = 编译期类型 switch
else {...}
```

规则细则：

- 丢弃分支的 `return` 不参与本实例的返回类型推导——
  各类型实例只见自己那条分支，多分支不同返回不再冲突
- 递归终点并进函数体：`if constexpr (N <= 1) return 1; else return N * f<N-1>();`
  ——不再需要单独写终点重载
- 配合 `[[maybe_unused]]` 压掉"某分支才用到的参数"的告警

## 三、实际应用与最佳实践

### 3.1 场景：advance 一体化（示例 ③-1，旗舰案例）

随机访问迭代器 `it += n` 一步到位；双向/前向迭代器循环 `++`。
一份代码，两种策略，实例化期选定——零运行期判断。

### 3.2 场景：按类型渲染/序列化

数字加 `[ ]`、字符串原样；或"有 describe() 调 describe，否则拼整数"——
能力探测式分支让泛型接口对调用方透明。

### 3.3 最佳实践清单

1. 模板里"按类型走不同逻辑"一律 `if constexpr`，SFINAE 只留给接口约束
2. 递归终点并入函数体，消灭多余重载
3. `else if constexpr` 链当"编译期 switch(type)"用，最多 4-5 层，
   再深就改特化（C++20 concepts 是后话，第 31 课）
4. 条件用 traits 的 `_v` 变量模板（第 03 课），别手写 `::value`
5. 模板外的 if constexpr 两分支都查——留给模板内使用

## 常见陷阱

1. **丢弃分支仍需语法正确**：token 层面的错误照样报错；
   非依赖构造（与模板参数无关的代码）也会被检查
2. 以为条件可以用运行期值——必须常量表达式；与 init 语句叠加时，
   **init 变量要参与条件就必须声明 `constexpr`**（普通变量的值进不了
   常量表达式，示例曾在此编译失败）
3. 非模板函数里用 if constexpr 求心安——两个分支都编译，该错还是错
4. 分支返回类型不同时，若某个实例真的走到两个分支（不可能，但要防
   误设计成运行期混合）——记住判定发生在实例化期
5. 超长 else if constexpr 链难维护——那是在手写 concepts，
   C++20 有正解（第 31 课预告）

## 完整示例

见 `13_if_constexpr.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++17 -Wall -Wextra "CPP17/13_if_constexpr编译期分支/13_if_constexpr.cpp" -o run && ./run
```

## 练习

1. 兑现第 03 课练习 1 的预告：用 if constexpr 写 `describe`
   （int/double/string 走不同分支）。
2. 把第 03 课 `print_tuple` 的 dummy 数组技巧换成 if constexpr 递归索引
   （`if constexpr (I + 1 < tuple_size_v<T>)` 决定是否打印逗号）。
3. 写泛型 `my_hash`：整型用 FNV-1a、字符串用 std::hash、其余 static_assert 报错。
4. 在模板函数的丢弃分支故意写一个依赖型的非法表达式
   （如对整型调 `.size()`），用 `-fsyntax-only` 验证它真的被跳过；
   再把 if constexpr 换成普通 if 看它报错——体会"丢弃"的分量。

## 参考

- 提案 P0292R2: If statements with constant expression
- cppreference: [if constexpr](https://zh.cppreference.com/w/cpp/language/if)
