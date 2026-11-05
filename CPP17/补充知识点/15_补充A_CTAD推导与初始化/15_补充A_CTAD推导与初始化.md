# 补充 15A · CTAD 推导与初始化（C++17）

> 阅读顺序为第 15 课之后，不占每日主线编号。
> 所需标准为 C++17。配套代码见 [15a_ctad_initialization.cpp](15a_ctad_initialization.cpp)。

## 一、底层原理

一次 CTAD 声明包含两个步骤。编译器先根据构造函数和推导指引确定类模板的实参，再用初始化列表构造该类型的对象。显式指引也可以为 C++17 的聚合类提供推导途径。

```cpp
template <class T>
struct Label { T text; };

template <class T>
Label(T) -> Label<T>;
Label(const char*) -> Label<std::string>;
```

`Label{"north"}` 先推导成 `Label<std::string>`。随后聚合初始化用字符串字面量构造 `text` 成员。推导指引决定类型，成员仍遵守普通初始化规则。若选出的成员类型需要发生窄化转换，大括号初始化会报错。

## 二、具体语法

| 表达式 | 推导类型 | 内容 |
|---|---|---|
| `std::vector elements{3, 7};` | `std::vector<int>` | 两个元素 3、7 |
| `std::vector repeated(3, 7);` | `std::vector<int>` | 三个元素 7、7、7 |

这两行推导出同一种容器，初始化方式决定元素个数。示例用 `static_assert` 检查类型，再打印元素个数和首元素。

## 三、实际应用与最佳实践

保存文本标签时，示例用 `const char* text` 触发非模板指引，生成持有 `std::string` 的标签。直接传入可写的字符数组时，通用指引推导出 `Label<char*>`，保存的是指针。修改同一字符数组后，前者显示 `gate`，后者显示 `Gate`。成员类型决定标签是否持有字符内容。

## 常见陷阱

- 显式指引仍参与重载决议。直接传可写的 `char[]` 时，通用指引可推导出 `char*`，匹配优于需要添加 `const` 的指引。
- 推导成功之后仍需满足所选类型的初始化条件。大括号对容器的元素列表尤其有影响。


## 完整示例

在仓库根目录编译运行，先预测输出，再对照源码注释。

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic "CPP17/补充知识点/15_补充A_CTAD推导与初始化/15a_ctad_initialization.cpp" -o run.exe
.\run.exe
```

## 练习

1. 加入 `Label(char*) -> Label<std::string>;`，验证可写字符数组也能推导为持有字符串的标签。
2. 加入 `Label(double) -> Label<int>;`，用 `Label{1.5}` 观察初始化阶段的诊断。

## 参考

- [C++17 草案 N4659 · 类模板参数推导候选](https://timsong-cpp.github.io/cppwp/n4659/over.match.class.deduct)
- [C++17 草案 N4659 · 聚合初始化](https://timsong-cpp.github.io/cppwp/n4659/dcl.init.aggr)

## 学习衔接

从 [第 15 课](../../15_类模板参数推导CTAD/15_类模板参数推导CTAD.md) 进入本节，其他补充内容见 [C++17 补充知识点目录](../README.md)。
