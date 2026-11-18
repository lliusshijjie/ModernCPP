# 第 55 课 · std::mdspan 多维数组视图（C++23）

> 所需标准：**C++23**（`-std=c++23`）｜ 讲解：本文件 ｜ 独立代码：`55_mdspan.cpp`
> ⚠ 本机 g++ 15.1.0（MSYS2）**尚无 `<mdspan>`**（libstdc++ 未实现）——
> 按课程约定，示例以**手写等价物 MdView**（接口对齐
> `std::mdspan<T, extents, layout_right>` 的核心面）演示，
> 并在文中如实标注；生产环境可换 MSVC / 含 mdspan 的标准库 /
> kokkos-mdspan 参考实现，用法一致
> 前置：第 37 课（std::span——mdspan 是它的多维推广）

## 一、底层原理

### 1.1 多维数据的"形状"该住在哪里

图像、矩阵、张量的存储几乎都是**一维连续块**（vector/裸数组），
"2 行 3 列"只是解释方式。旧世界两种坏答案：

- **形状硬编码在算法里**：`buf[r * cols + c]` 满天飞——
  cols 改名/传错，越界与错位静默发生
- **为每个形状定义包装类**：Matrix2x3、Image、Tensor……
  类型爆炸，算法无法泛化

**mdspan**（P0009，源自 Kokkos）给出正解：
**span（非拥有连续视图）+ extents（形状）+ layout（下标映射）**
三位一体。"数据在哪、什么形状、怎么算偏移"全部显式参数化。

### 1.2 layout：同一个块，两种读法

- `layout_right`（行主序 / C 惯例）：`offset = r * cols + c`
- `layout_left`（列主序 / Fortran、BLAS 惯例）：`offset = c * rows + r`

数学库要 Fortran 布局、图像管线是行主序——mdspan 让**同一块内存**
按不同布局被"看"，数据一个字节都不搬。转置视图 = 换映射视角
（示例 ②）。

### 1.3 operator() 而不是 operator[]：历史包袱的教训

C 里 `m[i, j]` 不是二维下标——**逗号运算符**取后者（`m[j]`）。
C++23 虽然允许多参 `operator[]`（第 58 课），但 mdspan 仍然选择
`m(i, j)` 函数调用语法：所有容器/所有标准一致、无逗号歧义、
还能与数学记法对齐（示例 ① 故意踩这个坑）。

### 1.4 extents：静态与动态混合

`extents<2, 3>` 形状编译期定死（更快的偏移计算）；
`extents<dynamic_extent, 3>` 行数运行期、列数编译期——
"通道数固定 3、样本数不定"这类半固定形状表达力拉满。
（手写版只演示全动态；标准版这才是精华。）

## 二、具体语法

```cpp
#include <mdspan>                                  // 本机暂缺，见文件头说明

std::vector<int> buf{1,2,3,4,5,6};
std::mdspan m(buf.data(), 2, 3);                   // 全动态 2x3，行主序

m(0, 0);  m(1, 2);                                 // 函数调用下标
m.extent(0);  m.extent(1);                         // 各维长度
m.stride(0);                                       // 跨步
m.data_handle();                                   // 底层指针（= data()）
m.size();                                          // 元素总数

std::mdspan<int, std::extents<2, 3>> fixed{...};   // 静态形状
std::mdspan<int, std::dextents<2>, std::layout_left>
    col_major{...};                                // 列主序视图
auto sub = std::submdspan(m, {1, 2}, {0, 3});      // 子块视图（P2630）
```

规则细则：

- mdspan **不拥有**数据：生命周期与 span 同规（第 37 课悬垂规则照搬）
- `operator()` 不查边界；越界 UB——形状自查（或 debug 断言）自理
- 元素要求：连续存储的"类 span"——vector/data()/数组均可接入
- 拷贝便宜（指针 + 形状 + 映射），按值传参
- `submdspan` 是配套切片（C++23 标准，本机同缺）——
  手写版用"指针偏移 + 新形状"模拟（示例 2.4）

## 三、实际应用与最佳实践

### 3.1 场景：科学计算与图像处理（示例 ③）

卷积窗口、行/列统计、分块处理——算法写"语义下标"，
形状由视图携带：换图像尺寸不改算法，换布局不改数据。

### 3.2 场景：与 C/BLAS 库交互

`layout_left` 视图直接喂 Fortran 系库（LAPACK），
`layout_right` 对接 C 系——边界处零拷贝零转换（MD 速览）。

### 3.3 最佳实践清单

1. 多维算法签名收 `mdspan<T>`（或泛型 `mdspan` 概念），
   别收"裸指针 + rows + cols"三件套
2. 形状能编译期定的用静态 extents——偏移计算可被优化成乘法常数
3. 视图不拥有：底层 vector 扩容/销毁后重建 mdspan（第 37 课心法）
4. 跨语言边界优先选对 layout（right/left），别事后再转置大块
5. 本机暂无 <mdspan> 时：手写等价物或 kokkos/mdspan 参考实现
   填缝，接口按标准写，迁移零成本

## 常见陷阱

1. **`m[1, 2]` 不是二维下标**（未重载多参 [] 的类型）：
   逗号表达式取后者——`m[2]`！C++23 起编译器警告
   （-Wcomma-subscript）；mdspan 用 `m(1, 2)` 避开百年老坑
2. 越界 `m(5, 0)` 是 UB 不是异常——形状自管（对照 span 陷阱）
3. 行主序数据配 `layout_left` 视图：读出来是转置——
   布局声明必须与真实存储一致，错了不报、只错值
4. mdspan 不延长底层生命期：临时 vector 的 data() 存进 mdspan
   = 经典悬垂（同第 37 课 span 的右值规则）
5. 静态 extents 与实际形状不符：构造时即 UB——
   静态形状只用于"编译期就有保证"的场合
6. 以为 mdspan 是容器：它没有 push_back/resize——
   形状是视图属性，改形状=新建视图

## 完整示例

见 `55_mdspan.cpp`（含手写 MdView 与本机可用性说明），按
① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++23 -Wall -Wextra "CPP23/55_std_mdspan多维数组视图/55_mdspan.cpp" -o run && ./run
```

## 练习

1. 给 MdView 加 `layout_left` 模板开关（或第二个类），
   同一块数据两种视图互相验证：`left(r,c) == right(c,r)`。
2. 扩展 MdView 到三维（深度×行×列），写一个"逐层求和"。
3. 把手写版换成 `std::mdspan` 的等价代码写进注释，
   待本机标准库支持后解开验证（迁移练习）。
4. 写 `submdspan` 手写版：参数为 (view, 行区间, 列区间)，
   返回新的 MdView——注意行区间非零起点时的指针偏移。

## 参考

- 提案 P0009R18（mdspan）/ P2630R4（submdspan）
- cppreference: [std::mdspan](https://zh.cppreference.com/w/cpp/container/mdspan)
- Kokkos mdspan 参考实现（github.com/kokkos/mdspan）
