# 第 12 课 · if 与 switch 的初始化语句（C++17）

> 所需标准：**C++17** ｜ 讲解：本文件 ｜ 独立代码：`12_if_init_statement.cpp`

## 一、底层原理

### 1.1 新语法只是"作用域手术"

```cpp
if (init; cond) A; else B;
```

编译器视角，它等价于一个现成的块：

```cpp
{
    init;                        // 求值一次，先于条件
    if (cond) A; else B;
}                                // init 声明的变量在这里死亡
```

两条规则记住即可：

- init 变量的作用域 = **整个 if/else 语句**——条件与**两个分支**都能看到
- 语句结束后完全不可见——作用域最小化，误用编译报错（这是保护）

### 1.2 它解决的旧问题：条件辅助变量的"两种死法"

| C++11/14 写法 | 死法 |
|---|---|
| `auto it = m.find(k); if (it != m.end()) {...}` | **泄漏**：it 活到函数尾，后续代码可误用 |
| `if (auto it = m.find(k) != m.end())` 或在 if 内声明 | **失明**：else 分支根本拿不到 it |

`if (init; cond)` 一举两得：作用域被限制，且 else 分支同样可用 it——
"找不到时报告键名"这种逻辑终于写得自然。

### 1.3 为什么 while 没有这个语法

提案讨论过 `while (init; cond)`，被拒绝：init 只求值一次，而循环条件
每轮都要重算——语义不搭。`while (std::getline(in, line))` 这类惯用法
继续用老写法就好。

## 二、具体语法

```cpp
if (decl-or-expr; cond) {...}          // init 可以是声明或表达式
switch (decl-or-expr; cond) {...}      // switch 同款
if (auto [a, b] = f(); a > b) {...}    // 与结构化绑定组合（黄金搭档）
```

规则细则：

- init 与 cond 之间的**分号必须有**——这一笔之差区分新旧两种语义
- init 里可以声明多个变量（逗号分隔）
- switch 的 init 在任何 case 匹配**之前**求值一次
- init 变量与外层同名会发生遮蔽，开 `-Wshadow` 可查

## 三、实际应用与最佳实践

### 3.1 场景三板斧

1. **查表双分支**：`if (auto it = m.find(k); it != m.end()) ... else 报告键名`
2. **insert 校验**：`if (auto [pos, ok] = m.insert(x); ok) ...`（第 11 课组合）
3. **错误码风格**：`if (auto ec = f(); ec) { 处理错误 }`——
   C++23 `std::expected`（第 50 课）出现前，这是错误处理的现代标准姿势

### 3.2 最佳实践清单

1. **仅为条件服务的变量必须放进 init**——作用域即文档
2. map 的 find/insert 惯用式背下来，肌肉记忆级别
3. 别把业务变量塞进 init：init 只放"条件辅助"变量
4. init 里避免逗号表达式副作用链——可读性优先，复杂逻辑提取成函数
5. 团队约定：一律写新形式，消灭"声明游离在 if 外"的旧风格

## 常见陷阱

1. **以为 else 分支看不到 init 变量**——恰好相反，两个分支都可见
2. **语句之后还想用 init 变量**——已死亡，编译错；这是设计不是 bug
3. **少写分号，语义全变**：`if (auto x = f())` 是 C++11 的条件声明
   （x 按 bool 上下文转换），与 `if (auto x = f(); x)` 完全不同——
   code review 时盯紧这个分号
4. **init 只求值一次**：想"每轮重算"的逻辑放进去是错的（while 没这语法不是疏忽）
5. switch 的 init 在所有 case 之前执行——不要在 case 里假设它能重复初始化

## 完整示例

见 `12_if_init_statement.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++17 -Wall -Wextra "CPP17/12_if与switch的初始化语句/12_if_init_statement.cpp" -o run && ./run
```

## 练习

1. 写函数 `find_port(const std::map<std::string,int>&, const std::string& key)`：
   用 init 语句风格实现"找到返回端口，找不到返回 -1"。
2. 用 `switch (init; cond)` 写 mini 命令解析器：'h' 帮助、'q' 退出、
   其他输出 unknown。
3. 用 `find_if` + init 语句 + 结构化绑定，一行定位 vector 中首个大于 100 的元素。
4. 构造 `if (auto x = f())` 与 `if (auto x = f(); x)` 的行为差异 demo，
   贴在团队 wiki 警示分号的重要性。

## 参考

- 提案 P0305R1: Selection statements with initializer
- cppreference: [if 语句（含初始化）](https://zh.cppreference.com/w/cpp/language/if)
