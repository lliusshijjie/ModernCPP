# 第 16 课 · std::optional（C++17）

> 所需标准：**C++17** ｜ 讲解：本文件 ｜ 独立代码：`16_optional.cpp`

## 一、底层原理

### 1.1 "T 或空"的值语义包装

`std::optional<T>` 的内存布局 ≈ 一块按 T 对齐的存储 + 一个"有值"标记，
**全部在栈上，零堆分配**。它是值的容器不是指针：可拷贝、可比较、可放
容器里——与 `unique_ptr<T>`（堆 + 空态）是完全不同的定位。

### 1.2 它取代的四种旧方案，各有各的坑

| 旧方案 | 问题 |
|---|---|
| 魔法值（`-1`/`""`/`nullptr`） | 侵蚀合法值域；调用方必须记得判断 |
| 出参 + bool | T 必须可默认构造；先声明再赋值的两段式 |
| 找不到就抛异常 | "没找到"是常态流程，不是异常 |
| `pair<T, bool>` | 语义弱，成员名 `.first/.second` 无信息 |

`optional<T>` 一次性解决：值域纯净、T **不要求默认构造**、常规空态零成本。

### 1.3 两态语义与比较规则

- 两态：engaged（有值）/ empty（空）；空态取值 = 未定义行为（`*`）或抛
  `bad_optional_access`（`value()`）
- 比较按"空 < 有值"全序：空的排在最前；两个空相等
- `std::nullopt` 是"空"的字面量

## 二、具体语法

```cpp
std::optional<int> o;          // 空
std::optional<int> o2 = 42;    // 有值（拷贝初始化）
o.reset();                     // 回到空
o.emplace(args);               // 原地构造（T 可无默认构造）
o = 9;                         // 赋值即置位
if (o) {...}                   // 上下文转 bool（= has_value()）
o.value();                     // 空则抛 bad_optional_access
*o;                            // 空则 UB —— 快，但必须先判
o.value_or(def);               // 空则给默认 —— 安全兜底
```

规则细则：

- T **不要求默认构造**——这是对比"出参风格"的核心优势
- `optional<T&>`（引用）不允许；要引用语义就用指针或 `optional<reference_wrapper<T>>`
- CTAD 可用：`std::optional o = 42;` → `optional<int>`（第 15 课的应用）
- 与第 12 课 init 语句是绝配：`if (auto r = find(k)) {...}`

## 三、实际应用与最佳实践

### 3.1 场景：查找型接口（示例 ③-1）

`find_user` 返回 `optional<User>` 而不是指针/魔法值/异常——
调用方用 `if (auto u = find_user(...))` 一行处理两条路。

### 3.2 场景：配置兜底（value_or）

`port = cfg.get("port").value_or(8080);` —— 一行表达"有就用，没有给默认"。

### 3.3 最佳实践清单

1. "可能没有结果"的函数返回 optional，禁用魔法值
2. 只读取值优先 `value_or`；确定有值才用 `*`（配合 if 判断）；`value()` 留给
   "空即是 bug"的场景（让异常暴露它）
3. 参数要"可选"时慎用 `optional<T>` 形参（语义模糊）——重载或默认参数通常更好
4. optional 里再套 optional 是设计坏味——用 variant 或重新建模（第 19 课对比）
5. 移出值：`std::move(*o)`，随后 o 仍处于 engaged 但值已搬空

## 常见陷阱

1. **对空 optional 解引用**（`*o`、`o->m`）是 UB——编译器不查，养成
   `if (o)` 的肌肉记忆
2. `value()` 抛的 `bad_optional_access` 若不接就 terminate——
   别在热路径上把它当流程控制
3. `optional<bool>`：`if (ob == true)` 之类比较歧义重重——先想是不是真需要它
4. `optional<T>` 的 `T` 是引用/数组不行；函数类型也不行
5. emplace/reset 后旧对象的析构时机：reset 立即析构——如果 T 持有资源，
   注意与预期析构顺序的配合

## 完整示例

见 `16_optional.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++17 -Wall -Wextra "CPP17/16_std_optional/16_optional.cpp" -o run && ./run
```

## 练习

1. 写 `parse_port(const std::string&)`：合法范围 1-65535 返回 `optional<int>`，
   否则 nullopt；调用处分别用 `if`、`value_or` 两种方式消费。
2. 把第 06 课 ConfigCache 的 `get` 改为返回 `optional<std::string>`，
   调用端改成 value_or 风格。
3. 用 `std::vector<std::optional<int>>` 模拟"有空洞的序列"，
   遍历输出非空值（练习 if + 解构 + optional 组合）。
4. 故意对空 optional 调 `.value()`，捕获 `bad_optional_access` 并打印
   what()（写完体会：这条异常路径为什么不该当流程用）。

## 参考

- 提案 N3672 / P0603 等 optional 系列
- cppreference: [std::optional](https://zh.cppreference.com/w/cpp/utility/optional)
- 《Effective Modern C++》无直接条目；C++ Core Guidelines E.13 / F.21 相关
