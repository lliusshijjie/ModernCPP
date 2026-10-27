# 第 07 课 · decltype(auto) 与返回类型推导（C++14）

> 所需标准：C++14 ｜ 讲解：本文件 ｜ 独立代码：`07_decltype_auto.cpp`

## 一、底层原理

### 1.1 auto 的返回类型推导用的是"按值模板形参"规则

C++14 允许函数用 `auto` 推导返回类型，但规则**不是**"表达式的类型"，
而是按值模板形参的模板实参推导：**引用被剥掉、cv 被剥掉、数组退化为指针**。

| return 表达式 | `auto` 推导 | `decltype(auto)` 推导 |
|---|---|---|
| `vec[i]`（`int&`） | `int`（副本！） | `int&` |
| `const int&` | `int` | `const int&` |
| `std::move(s)` | `std::string` | `std::string&&` |
| `x + 1`（纯右值） | `int` | `int` |

后果：想写"转发下标/返回引用"的封装，`auto` 会把引用**静默吞成副本**——
不报错、能编译、语义悄悄变了。C++11 的解法是尾置声明
`-> decltype(expr)`（表达式要手抄一遍）；C++14 的 `decltype(auto)`
让 decltype 的规则直接接管推导，一处关键字搞定。

### 1.2 decltype 的两套规则：一切"魔法"的根源

| 写法 | 采用的规则 | 结果 |
|---|---|---|
| `decltype(g)`（**实体**：未加括号的 id 表达式） | 声明类型 | `int` |
| `decltype((g))`（**表达式**） | 按值类别：左值→`T&`，纯右值→`T`，将亡值→`T&&` | `int&` |

`decltype(auto)` 走的是**表达式**那套规则。于是：
`return g;` 推出 `int`，`return (g);` 推出 `int&`——**一个括号改变了返回类型**。

### 1.3 括号 + 局部变量 = 悬空引用

```cpp
decltype(auto) bad() {
    int local = 0;
    return (local);      // int& → 悬空引用！-Wall 会警告，但别靠它兜底
}
```

这是《Effective Modern C++》Item 3 的著名案例。本课示例只演示**全局变量**
的安全版本（语义一样，不会悬空）。

## 二、具体语法

### 2.1 两种形式

```cpp
decltype(auto) f();              // 函数返回类型（前/尾置皆可）
decltype(auto) x = expr;         // 变量声明（C++14 同样新增）
```

### 2.2 规则细则

- 必须有初始化器；多条 `return` 必须推导出**完全相同**的类型
- `decltype(auto)` 是一个整体，不能与 `auto`、`const`、`&` 组合修饰
- 引用/指针层级、cv 完整保留——这正是它与 auto 的唯一区别，也是全部价值

## 三、实际应用与最佳实践

### 3.1 场景：完美转发包装（保住被调函数的返回类别）

```cpp
template <class F, class... Args>
decltype(auto) invoke_forward(F&& f, Args&&... args) {
    return std::forward<F>(f)(std::forward<Args>(args)...);
}
```

`f` 返回 `int&`，包装也返回 `int&`——赋值、取地址、无拷贝，全 preserved。
这是 C++14 写通用转发层的标准答案（C++11 要手抄一遍 `-> decltype(...)`）。

### 3.2 场景：通用下标包装

```cpp
template <class Container>
struct At {
    Container& c;
    template <class I>
    decltype(auto) operator[](I i) { return c[i]; }   // 保引用
};
```

`av[i] = x` 直写容器，零拷贝——用 `auto` 的话赋值语句直接编译失败。

### 3.3 最佳实践清单

1. **转发/包装层一律 `decltype(auto)`**；auto 会静默改变值/引用语义
2. 普通业务函数返回新值 → 用 `auto` 就好，别滥用 decltype(auto)
3. 用 `static_assert` + `std::is_same` 当推导结果的单元测试（示例全程在用）
4. 括号不是装饰：`return (x)` 与 `return x` 类型可能不同——统一风格，少用括号
5. C++14 起 wrapper 不要再手写尾置 `-> decltype(f(x))`，直接 decltype(auto)

## 常见陷阱

1. `return (local);` → 悬空引用（-Wall 有警告，但要靠 code review 兜底）
2. 以为 auto 只是"少打类型"——它在按值推导，语义可能已变
3. `decltype(auto) x = {1};` 直接编译错误（decltype 对初始化列表无规则）
4. `vector<bool>::operator[]` 返回**代理对象**（纯右值），decltype(auto) 推出
   的是代理的 `&&` 而非 `bool&`——期待别落空
5. 多条 return 的表达式形式不一致（一条带括号、一条不带）→ 推导冲突编译错误

## 完整示例

见 `07_decltype_auto.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++14 -Wall -Wextra "CPP14/07_decltype(auto)与返回类型推导/07_decltype_auto.cpp" -o run && ./run
```

## 练习

1. 写 `const` 版 front/back 包装：参数 `const std::vector<int>&`，
   用 `static_assert` 验证返回 `const int&`。
2. 故意写 `return (local);`，编译看 `-Wall` 的警告文本长什么样
   （写完就删，别放进工程）。
3. 实验 `vector<bool>`：`decltype(auto) b = vb[0];` 推出什么类型？
   用 `static_assert` + 第 03 课的 `type_name` 打印确认。
4. 把第 04 课的 `make_widget` 工厂改成返回 `decltype(auto)` 会发生什么？
   思考后动手验证（提示：返回类型从值变成了什么？是否还安全？）。

## 参考

- 《Effective Modern C++》Item 3：理解 decltype——本课的直接理论来源
- cppreference: [decltype](https://zh.cppreference.com/w/cpp/language/decltype)
- cppreference: [返回类型推导](https://zh.cppreference.com/w/cpp/language/function#_.E8.BF.94.E5.9B.9E.E7.B1.BB.E5.9E.8B.E6.8E.A8.E5.AF.BC)
