# 第 15 课 · 类模板参数推导 CTAD（C++17）

> 所需标准：**C++17** ｜ 讲解：本文件 ｜ 独立代码：`15_ctad.cpp`

## 一、底层原理

### 1.1 为什么以前必须写模板参数

C++ 的**函数模板**自古就能从实参推导类型，但**类模板**不能——
所以标准库造了一堆"推导辅助函数"：`make_pair`、`make_tuple`、`make_shared`……
它们存在的唯一理由就是类名后不能省尖括号。

C++17 的 CTAD 补上这块：写 `std::pair p{1, 2.5}` 时，编译器把每个构造函数
当作一个"虚构的函数模板"参与重载决议：

```cpp
// 编译器为 pair 的构造函数隐式合成（示意）：
template <class T1, class T2>
std::pair<T1, T2> __guide(const T1&, const T2&);   // pair(x, y) -> pair<T1, T2>
```

于是**类的构造过程变成了函数推导过程**——没有运行期机制，
纯编译期重载决议。

### 1.2 指引的两个来源

| 来源 | 说明 | 例 |
|---|---|---|
| 隐式指引 | 从每个构造函数自动合成 | `pair`、`tuple`、自定义带 T 构造的类 |
| 显式指引 | 库作者手写 `构造签名 -> 目标特化` | `vector`、`lock_guard` |

`vector` 为什么需要显式指引？因为 `vector v(1, 2)` 有歧义——
`(size_type, const T&)` 填充构造与 `(InputIt, InputIt)` 迭代器对构造
都能匹配 `(1, 2)`。库里手写的指引告诉编译器"两个整数按填充构造理解"。
`lock_guard` 更极端：构造函数里根本没有可推 T 的实参，只能手写
`lock_guard(mutex_type&) -> lock_guard<mutex_type>`。

### 1.3 显式指引优先于隐式指引

两者同时候选时，显式指引胜出。这也是库修正推导结果的手段。

## 二、具体语法

```cpp
std::pair p{1, 2.5};        // pair<int, double>
std::tuple t(1, 2.5, 'c');  // tuple<int, double, char>
std::vector v{1, 2, 3};     // vector<int>（经显式指引）
std::vector w = v;          // 拷贝推导：vector<int>
std::lock_guard lg(m);      // lock_guard<mutex>
```

规则细则：

- CTAD 要求**完全不写**模板实参——`std::vector<> v{1}` 在 C++17 是错的
  （C++20 起才允许部分显式）
- 大括号列表里类型打架（`{1, 2.5}`）→ 推导歧义编译错，这是保护不是麻烦
- 无构造函数的聚合体（裸 struct）C++17 还不能 CTAD——那是 C++20 的能力
- 拷贝推导：初始化器是同一模板的实例时直接沿用其参数

## 三、实际应用与最佳实践

### 3.1 场景：make_* 家族大面积退休

`make_pair/make_tuple` 在纯推导场景下不必再写（`make_shared/make_unique`
保留——它们还有"单次分配/异常安全"的独立价值，见第 04 课）。

### 3.2 场景：给自己的类写指引

带 T 构造函数的类自动获得隐式指引；构造函数"不含 T"（如只有长度参数）
的类，手写一条显式指引即可让用户少敲类型——示例 ③-2 演示语法。

### 3.3 最佳实践清单

1. 局部对象优先 CTAD：`std::pair p{...}` 替代 `std::pair<int,...>` 长声明
2. 推导失败（歧义）时检查实参类型是否混型，而不是加回显式模板参数
3. 自定义类模板设计构造函数时想好"哪个实参携带 T 的类型信息"
4. 库代码给类模板补显式指引，是 C++17 起的标配动作
5. `auto p = std::pair{1, 2.5}` 与直接 CTAD 等价，选一种团队风格

## 常见陷阱

1. **混型大括号推导失败**：`std::vector v{1, 2.5}` 歧义编译错——
   这是有意的；真要存 double 就统一实参类型
2. 以为聚合体（无构造的 struct）也能 CTAD——C++17 不行，C++20 才行
3. 零参构造推不出任何东西：`std::vector v;` 不是 CTAD，是默认构造
   （当然，没有模板参数的 vector 也写不出来）——区分"默认构造"与"推导"
4. 自定义指引签名与构造函数不匹配 → 候选直接落选，错误信息指向
   "no matching function"，别误以为指引语法错
5. CTAD 也会推导出"合法但意外"的类型：`vector v{1, 2, 3}` 是 vector<int>，
   `vector v{'a', 'b'}` 是 vector<char>——以实参类型为准，与直觉对齐后再写

## 完整示例

见 `15_ctad.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++17 -Wall -Wextra "CPP17/15_类模板参数推导CTAD/15_ctad.cpp" -o run && ./run
```

## 练习

1. 用 `static_assert` 验证 `std::tuple t(1, 2.5, 'c')` 的完整类型。
2. 给第 04 课的 `Widget` 包一层类模板 `Box<T>`，让 `Box b{42}` 自动推导，
   再故意写 `Box b{42, 3.14}` 观察歧义报错。
3. 为 `Box` 写显式指引 `Box(T, U) -> Box<T>`，验证 `Box b{1, 2}` 现在推导为
   `Box<int>`（显式指引优先）。
4. 把第 12 课示例里的 `std::pair<int, std::string>` 显式声明改为 CTAD 风格。

## 参考

- 提案 P0091R3 / P0067R5: CTAD
- cppreference: [类模板推导](https://zh.cppreference.com/w/cpp/language/class_template_argument_deduction)
- cppreference: [推导指引](https://zh.cppreference.com/w/cpp/language/class_template_argument_deduction#.E6.8E.A8.E5.AF.BC.E6.8C.87.E5.BC.95)
