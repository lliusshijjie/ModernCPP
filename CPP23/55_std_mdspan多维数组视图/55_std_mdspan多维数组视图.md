# 第 55 课 · std::mdspan 多维数组视图（C++23）

> 标准：C++23；代码：`55_mdspan.cpp`；前置：第 37 课 `span`。
> 本机 GCC 15.1.0/libstdc++ 没有 `<mdspan>`，示例启用二维教学类 `MdView`。
> 检测到标准头与 `__cpp_lib_mdspan >= 202207L` 时，同一示例使用真实 `std::mdspan`。
> 教学类只模拟动态二维、步长与指针访问，不是标准类型的等价实现。

## 一、底层原理

### 1.1 数据、形状、映射、访问器

`mdspan<ElementType, Extents, LayoutPolicy, AccessorPolicy>` 将多维下标映射到存储。
它不拥有元素；默认访问器使用指针，但自定义访问器还能支持其他数据句柄和访问方式。
不能把所有 mdspan 都限定为连续数组；带步长的子块可能存在间隙。

默认 `layout_right` 的二维映射为 `r * cols + c`，最右维步长为 1；
`layout_left` 为 `c * rows + r`，最左维步长为 1；`layout_stride` 显式保存各维步长。
`size()` 是逻辑元素数量，带间隙的映射所需存储跨度可能更大。

### 1.2 标准下标是 `m[r, c]`

**C++23 的 `std::mdspan` 使用多参数 `operator[]`，没有标准 `operator()` 下标。**
早期提案或第三方移植可能提供圆括号接口，需要按版本区分。
内建数组和 `vector` 没有二维下标：`raw[i, j]` 在标准 C++23 中不合法，
更早版本才把顶层逗号当作逗号表达式（C++20 已弃用）。GCC 接受旧写法可能是兼容扩展。

### 1.3 形状类型带索引类型

```cpp
using Fixed = std::extents<std::size_t, 2, 3>;
using Mixed = std::extents<std::size_t, std::dynamic_extent, 3>;
using Dynamic2D = std::dextents<std::size_t, 2>;
```

不能省略 `extents` 或 `dextents` 的第一个类型参数。
静态形状让部分计算可在编译期完成，但不承诺总比动态形状更快。
标准类型的 `Extents` 是必需模板参数，不能直接把 `mdspan<T>` 当完整类型。

## 二、具体语法

以下是标准 C++23 写法（需要支持 `<mdspan>` 的标准库）：

```cpp
#include <array>
#include <cstddef>
#include <mdspan>
#include <vector>

std::vector<int> buf{1, 2, 3, 4, 5, 6};
std::mdspan m(buf.data(), 2, 3);                    // CTAD：动态二维、layout_right
m[0, 1] = 20;
std::mdspan<int, std::extents<std::size_t, 2, 3>> fixed(buf.data());
std::mdspan<int, std::dextents<std::size_t, 2>, std::layout_left>
    transposed(buf.data(), 3, 2);                  // 形状也交换，才对应转置
// transposed[c, r] == m[r, c]

using E = std::dextents<std::size_t, 2>;
using M = std::layout_stride::mapping<E>;
std::mdspan<int, E, std::layout_stride> sub{
    buf.data() + 1, M{E{2, 2}, std::array<std::size_t, 2>{3, 1}}};
// 子块为 20 3 / 5 6，行步长仍是原矩阵的 3。
```

`extent(d)` 查各维长度，`stride(d)` 查可步长映射的步长，`data_handle()` 返回访问器的数据句柄，
`mapping()` 与 `accessor()` 分别提供映射与访问器。默认句柄是指针，不能泛化成所有类型都有 `data()`。
mdspan 没有标准 `begin/end`，本身不满足 ranges 的 `range` 概念，也不是容器。

`const mdspan<int, E>` 固定视图对象，仍可以修改 `int`；只读元素要写 `mdspan<const int, E>`。
默认访问器下 `operator[]` 自身为 const，返回类型取决于元素类型。

## 三、实际应用与最佳实践

### 3.1 子块必须保留原步长

从 2×3 矩阵取右侧 2×2 子块时，第一行和第二行的起点仍相差 3 个元素。
只做“偏移指针 + 新行列数”并按紧密 2×2 访问会错误地读到中间元素。
示例用 `layout_stride` 或教学类中的两维步长表达这一点。

`std::submdspan` 的标准归属是 **C++26**，不是 C++23。
C++23 可手动组合偏移、extents 和映射；第三方库提供的切片接口须按其文档使用。

### 3.2 转置与外部库

转置应同时交换形状与对应步长。把同样的 2×3 形状从 right 换成 left 只是另一种解释，
通常不是原矩阵的转置。映射不会搬动数据，也不会自动把错误布局变正确。
调用 BLAS/LAPACK 还要传符合该 API 的指针、维度和 leading dimension，
不能直接把 mdspan 对象当作 C/Fortran 形参。

### 3.3 生命周期与前置条件

底层存储必须存在，且覆盖 `mapping().required_span_size()` 所需的访问区域。
mdspan 无法仅凭指针检查分配长度；默认访问器的下标没有抛出越界异常的保证。
形状、步长与索引应满足各自前置条件，维度乘积和偏移必须可表示。
vector 重分配或销毁会使借用失效；修改元素但不使存储失效则会被视图观察到。

## 常见陷阱

1. `m(r, c)` 不是标准 mdspan 下标；用 `m[r, c]`。
2. `extents<2, 3>`、`dextents<2>` 少了索引类型，不能编译。
3. 只把视图声明为 const 不会把元素变成 const。
4. 静态 extent 与构造参数冲突可能违反前置条件；底层存储不足也不会自动得到异常。
5. `size()` 不总等于需要分配的连续存储跨度；子块和带间隙的布局尤其要注意。
6. 教学类不覆盖完整标准约束、静态 extents、自定义访问器和全部构造重载，不能据此承诺零成本迁移。

## 完整示例

```bash
g++ -std=c++23 -Wall -Wextra "CPP23/55_std_mdspan多维数组视图/55_mdspan.cpp" -o run && ./run
```

程序会打印 `backend=...`，标明当前测试的是标准类型还是教学类。
本机 MSVC 19.44 的标准类型路径也可使用 `/std:c++latest /utf-8 /EHsc` 编译验证。

## 练习

1. 用 2×3 right 和 3×2 left 视图验证转置下标对应，写穿后检查同一底层元素。
2. 取跨多行的非完整列子块，检查步长与逻辑元素数量。
3. 对比 const 视图与 const 元素；保留只读元素赋值为预期编译失败测试。
4. 在支持标准库上测试静态/动态混合 extents，避免运行越界或悬垂代码。

## 参考

- [C++23 草案：多维访问类型与布局](https://timsong-cpp.github.io/cppwp/n4950/views.multidim)
- [C++23 草案：mdspan 元素访问](https://timsong-cpp.github.io/cppwp/n4950/mdspan.mdspan.members)
- [P2630R4：从 C++23 mdspan 分离的 submdspan](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2630r4.html)
