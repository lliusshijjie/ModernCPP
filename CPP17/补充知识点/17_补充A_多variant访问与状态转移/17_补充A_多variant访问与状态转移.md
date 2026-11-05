# 补充 17A · 多 variant 访问与状态转移（C++17）

> 阅读顺序为第 17 课之后，不占每日主线编号。
> 所需标准为 C++17。配套代码见 [17a_multi_variant_visit.cpp](17a_multi_variant_visit.cpp)。

## 一、底层原理

`std::visit(visitor, state, event)` 同时接受当前状态和事件。编译器检查每一种备选组合能否调用访问器，运行期将当前两个活动值传给它。示例中状态和事件各有三种类型，因此共有 9 种组合需要覆盖。

## 二、具体语法

| 当前状态 | 事件 | 新状态 |
|---|---|---|
| `Idle` | `Start` | `Running{0}` |
| `Running{n}` | `Chunk` | `Running{n + 1}` |
| `Running{n}` | `Finish` | `Finished{n}` |
| 其他组合 | 对应事件 | 返回 `nullopt` |

三个具体 lambda 都接收 const 引用，泛型兜底也接收 const 引用。匹配合法组合时，具体重载胜出。其余组合由兜底重载返回 `nullopt`。所有分支显式返回 `std::optional<State>`，满足 C++17 的 `visit` 对返回类型和值类别一致的要求。

## 三、实际应用与最佳实践

上传任务只在运行期间接收分块。示例先调用 `next_state` 计算新状态，再在外层替换当前状态。这样旧状态的引用只在计算期间使用，非法事件也会保留原状态。

```cpp
if (auto next = next_state(state, event); next) {
    state = *next;
}
```

演示接收两个分块后结束。随后到来的分块被拒绝，最终结果仍为 `finished chunks=2`。

## 常见陷阱

- 两个 variant 的备选数相乘。状态或事件种类增加时，先写清楚哪些组合允许转移。
- 泛型兜底让新组合也能通过可调用性检查。本例明确规定未列出的组合一律拒绝。如果新事件必须被接受，需要补上对应规则。
- 从访问器返回旧状态内部的引用后，替换状态可能使引用悬空。本例返回独立的新状态。


## 完整示例

在仓库根目录编译运行，先预测输出，再对照源码注释。

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic "CPP17/补充知识点/17_补充A_多variant访问与状态转移/17a_multi_variant_visit.cpp" -o run.exe
.\run.exe
```

## 练习

1. 添加 `Cancel` 事件，让 `Running` 接到它时回到 `Idle`，并试一下 `Finished + Cancel`。
2. 暂时移除泛型兜底，观察缺少组合时的编译诊断。

## 参考

- [C++17 草案 N4659 · variant 访问与全部备选组合](https://timsong-cpp.github.io/cppwp/n4659/variant.visit)

## 学习衔接

从 [第 17 课](../../17_std_variant与std_visit/17_std_variant与std_visit.md) 进入本节，其他补充内容见 [C++17 补充知识点目录](../README.md)。
