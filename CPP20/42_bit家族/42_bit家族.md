# 第 42 课 · <bit> 家族：popcount、bit_cast、endian（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`42_bit_family.cpp`
> 前置：第 30 课（std::byte——字节类型化）；第 24 课（charconv——底层性能视角）

## 一、底层原理

### 1.1 位操作：从"手写循环 + 内建函数"到标准件

数一个整数的 1、找前导 0、对齐到 2 的幂——旧世界三种写法：手写移位循环
（可读性差、优化看运气）、编译器内建 `__builtin_popcount`（不可移植）、
C 风格位技巧（`x & (x-1)`，聪明但易错）。`<bit>` 把这些**收编为标准件**，
编译器可根据目标指令集选用 `popcnt`/`lzcnt`/`tzcnt` 等指令或其他实现，
标准不保证每个函数都只生成一条指令。

### 1.2 只收无符号：约束而非未定义

位计数、循环移位和二次幂函数以**无符号整数类型**为约束，
不包括 bool、char、char8_t 等独立字符类型；传 int 不会自动转换后匹配。
C++20 已规定有符号整数采用补码表示，这些接口的无符号约束仍然存在。
若要观察有符号类型的位，可转成对应的 `std::make_unsigned_t<T>`；
转换按模 2^N 得到对应位值。转成不同宽度的 unsigned 则可能截断或扩展，
不能一概说"位模式不变、语义零损失"。`bit_cast` 本身没有无符号限制。

### 1.3 bit_cast：类型双关的合法通道

`float` 的位级内容想看/想传/想哈希，旧世界两条路：
`memcpy`（合法但啰嗦、非 constexpr）与指针双关
`*reinterpret_cast<uint32_t*>(&f)`（**严格别名 UB**）。
`std::bit_cast<To>(from)` 要求两类型**同样大小且都平凡可拷贝**，
按位复制形成一个新的 To 值。memcpy 仍是合法的表示复制手段；
bit_cast 更直接，且在满足额外限制时可用于常量求值（示例 ②）。

### 1.4 std::endian：端序的编译期答案

`std::endian::native / little / big`——一个枚举一次比较，
替代 `__BYTE_ORDER__` 宏与运行时探针。本机 x86/x64 是小端，
网络序是大端：跨机协议字段序列化前必查（示例 ①）。

## 二、具体语法

```cpp
std::popcount(x);        // 1 的个数
std::countl_zero(x);     // 前导 0（x=0 时 = 位数；x=1 时 = 位数-1）
std::countr_zero(x);     // 尾部 0（x=0 时 = 位数）
std::countl_one(x);      // 前导 1
std::countr_one(x);      // 尾部 1
std::bit_width(x);       // 最高位位置 + 1（bit_width(0)==0）
std::has_single_bit(x);  // 是 2 的幂吗
std::bit_floor(x);       // ≤ x 的最大 2 幂
std::bit_ceil(x);        // ≥ x 的最小 2 幂
std::rotl(x, n);  std::rotr(x, n);   // 循环移位（n 自动取模位数）
std::bit_cast<To>(from); // 位级重解释（同大小、平凡可拷贝、constexpr）

constexpr bool little = std::endian::native == std::endian::little;
// 传有符号实参：std::popcount(int_v) 编译错——先转无符号
```

规则细则：

- 全部函数 `constexpr`——进 `static_assert`、配合第 40 课 consteval
- `countl_zero(0u)` 定义为对应无符号类型的**位数**（32 位类型得 32），不是 UB
- `bit_floor(0u) == 0`，但 **`bit_ceil(0u) == 1`**，`bit_ceil(1u) == 1`
- `bit_ceil(uint32_t{0x80000000})` 本身可表示；输入 **0x80000001** 时
  结果需 2^32，不可表示，违反前提而产生 UB，并非保证按无符号规则回绕。
  常量求值中这类调用不合法
- `bit_cast` 的结果填充位未指定；若来源填充/未定值参与目标的值表示，
  不能任意读取并用于比较或哈希，需遵守对应规则。若位模式不对应合法目标值，
  C++20 中可能 UB
- bit_cast 的常量求值还有额外限制：相关类型及子对象不能含指针、
  成员指针、union、volatile 或引用成员等
- `rotl(uint8_t, 33)` 合法：移位数对位数取模
- C++23 追加了 `std::byteswap`（第 56 课顺带提）

## 三、实际应用与最佳实践

### 3.1 场景：对齐与分桶（示例 ③）

内存分配器的 size class、哈希桶下标、TLS 槽位——
当 n>=1 时，`bit_width(n-1)` 就是 ceil(log2(n))，取代手写循环与
`std::ceil(std::log2(n))`（后者还引入浮点误差与库依赖）。

### 3.2 场景：浮点位级剖析（示例 ③）

`bit_cast<uint32_t>(float)` 后按 IEEE754 拆符号/指数/尾数——
教学、调试 NaN 载荷、浮点哈希的标准起手式。

### 3.3 最佳实践清单

1. 位计数、前导零和二次幂取整优先使用 <bit>，明确参数类型与边界
2. 同大小平凡可拷贝类型之间的按位值转换优先用 bit_cast；
   memcpy 仍可合法复制平凡对象表示，不能据此把非平凡对象当字节随意复制
3. 协议规定字节序时显式编码/解码；若实现仅支持某种本机端序，
   可用 static_assert 明确该限制。native 也可能既非 little 也非 big（混合端序）
4. 位集密度（布隆过滤器、位图统计）用 `popcount`；
   稀疏检测用 `has_single_bit`
5. 有符号数参与位运算前**显式转无符号**——既是 `<bit>` 的要求，
   也是避开符号位陷阱（第 46 课 char 的教训同理）

## 常见陷阱

1. 位计数/循环移位/二次幂函数传有符号实参会被约束拒绝；
   bit_cast 和 endian 不能概括为"只收无符号"
2. `countl_zero(uint32_t{0})` 返回 32（位数）而不是"没有前导零"或 UB——
   边界值语义专门定义过，别按直觉猜
3. `bit_cast` 两个类型**大小不同** → 编译错；结构体有 padding 时
   padding 位内容不确定——比较/哈希要意识到
4. bit_ceil 结果不可表示时是 UB；调用前检查输入范围，
   不要把某次运行看到的小值当成标准规定的回绕结果
5. 循环移位 `x << n | x >> (32-n)` 手写版在 n=0 时右移 32 位是 UB；
   `rotl` 没有这个问题——又一个"标准件救全家"
6. endian 只报告本机表示，不会自动把数据转成网络序；
   对不同端序做分支处理与只支持一种端序的 static_assert 都可按需求选择

## 完整示例

见 `42_bit_family.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra "CPP20/42_bit家族/42_bit_family.cpp" -o run && ./run
```

## 练习

1. 用 `popcount` 写汉明距离：两个 uint32_t 不同的位有几个？
   再用 `countl_zero` 实现最长公共前缀（按位）。
2. `bit_cast` 拆一个 double 的 IEEE754 字段（1+11+52 位），
   打印 1.0 / -2.0 / 0.5 的 (符号, 指数, 尾数) 三元组。
3. 区分"取到二次幂"与"按 a 的倍数对齐"：如 n=13、a=4，
   前者向下得 8，后者向下得 12。写 align_down(n,a)/align_up(n,a)，
   验证 a 是非零二次幂，并检查向上对齐的加法溢出。
4. `static_assert` 验证：`rotl(rotl(x, 13), 19) == rotl(x, 32)` 对
   任意 uint32_t 成立（32 位取模归零）。

## 参考

- [C++20 草案：二次幂函数的边界与前提](https://timsong-cpp.github.io/cppwp/n4861/bit.pow.two)
- [C++20 草案：整数表示](https://timsong-cpp.github.io/cppwp/n4861/basic.fundamental)
- [C++20 草案：bit_cast 的限制](https://timsong-cpp.github.io/cppwp/n4861/bit.cast)
- 提案 P0553R4（位操作）/ P0476R2（bit_cast）
- cppreference: [头文件 <bit>](https://zh.cppreference.com/w/cpp/header/bit)
- cppreference: [std::bit_cast](https://zh.cppreference.com/w/cpp/numeric/bit_cast)
