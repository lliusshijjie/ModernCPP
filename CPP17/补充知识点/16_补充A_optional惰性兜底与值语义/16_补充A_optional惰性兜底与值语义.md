# 补充 16A · optional 惰性兜底与值语义（C++17）

> 阅读顺序为第 16 课之后，不占每日主线编号。
> 所需标准为 C++17。配套代码见 [16a_optional_lazy_fallback.cpp](16a_optional_lazy_fallback.cpp)。

## 一、底层原理

`title.value_or(build_default())` 会先调用 `build_default()`，把结果作为实参传给 `value_or`。进入函数后，`value_or` 才根据 `title` 的状态选择返回哪个值。示例中的标题已经存在，仍然观察到默认值构造次数为 1。

## 二、具体语法

```cpp
auto eager = title.value_or(build_default());
auto lazy  = title ? *title : build_default();
```

| 写法 | 标题存在时 | 标题缺失时 |
|---|---|---|
| `value_or(build_default())` | 执行 `build_default()`，再返回已有标题的副本 | 执行 `build_default()` 并使用其结果 |
| 条件表达式 | 复制已有标题 | 才调用 `build_default()` |

条件运算符只求值被选中的分支。默认值是现成常量时，`value_or` 写起来直接。默认值需要计算或有可观察副作用时，可以用条件表达式或显式 `if` 表达延迟执行。

## 三、实际应用与最佳实践

展览导览可以提供自定义标题。示例的 `display_title` 返回独立字符串，调用方保存结果后可以清空原来的 optional。自定义标题缺失时才生成默认标题。演示输出依次验证了有值路径、空值路径，以及清空原对象后仍可读取副本。

## 常见陷阱

- `const auto& ref = *title` 借用的是 optional 内部对象。先检查存在性，并在 `reset()` 之前用完引用。
- 对左值调用 `value_or` 会按值返回。仅需在当前作用域查看已有的大对象时，可以先检查有值，再借用其引用。


## 完整示例

在仓库根目录编译运行，先预测输出，再对照源码注释。

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic "CPP17/补充知识点/16_补充A_optional惰性兜底与值语义/16a_optional_lazy_fallback.cpp" -o run.exe
.\run.exe
```

## 练习

1. 在 `build_default` 中加入一条输出，比较标题存在时两种写法的调用次数。
2. 把自定义标题改为空字符串，观察有值的空字符串和 `nullopt` 在导览逻辑中的区别。

## 参考

- [C++17 草案 N4659 · optional 访问接口](https://timsong-cpp.github.io/cppwp/n4659/optional.observe)
- [C++17 草案 N4659 · 函数调用与实参求值](https://timsong-cpp.github.io/cppwp/n4659/expr.call)

## 学习衔接

从 [第 16 课](../../16_std_optional/16_std_optional.md) 进入本节，其他补充内容见 [C++17 补充知识点目录](../README.md)。
