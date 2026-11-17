# 第 42 课 · <bit> 家族：popcount、bit_cast、endian（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`42_bit_family.cpp`
> 前置：第 30 课（std::byte——字节类型化）；第 24 课（charconv——底层性能视角）

## 一、底层原理

### 1.1 位操作：从"手写循环 + 内建函数"到标准件

数一个整数的 1、找前导 0、对齐到 2 的幂——旧世界三种写法：手写移位循环
（可读性差、优化看运气）、编译器内建 `__builtin_popcount`（不可移植）、
C 风格位技巧（`x & (x-1)`，聪明但易错）。`<bit>` 把这些**收编为标准件**，
编译器直接映射到单条指令（x86 的 `popcnt`/`lzcnt`/`tzcnt`）。

### 1.2 只收无符号：约束而非未定义

`popcount/countl_zero/...` 全部以**无符号整数类型**为约束——传 `int`
直接编译错。原因：有符号数的位表示留有实现自由（补码只是事实标准），
而位计数要求位模式明确。想数有符号值先 `static_cast<unsigned>`
（位模式不变），语义零损失。

### 1.3 bit_cast：类型双关的合法通道

`float` 的位级内容想看/想传/想哈希，旧世界两条路：
`memcpy`（合法但啰嗦、非 constexpr）与指针双关
`*reinterpret_cast<uint32_t*>(&f)`（**严格别名 UB**）。
`std::bit_cast<To>(from)` 要求两类型**同样大小且都平凡可拷贝**，
按位复制、**constexpr 可用**——双关从此有且仅有一条正道（示例 ②）。

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
- `countl_zero(0)` 定义为**位数**（32 位类型得 32），不是 UB
- `bit_floor(0)/bit_ceil(0)` 均为 0
- `bit_ceil` 可能**溢出**（对 2^31 做 ceil 到 2^32 放不进 uint32_t）
- `bit_cast` 不剥 padding：结构体有填充位时那些位**值不确定**
- `rotl(uint8_t, 33)` 合法：移位数对位数取模
- C++23 追加了 `std::byteswap`（第 56 课顺带提）

## 三、实际应用与最佳实践

### 3.1 场景：对齐与分桶（示例 ③）

内存分配器的 size class、哈希桶下标、TLS 槽位——
`bit_width(n-1)` 一行就是"log2 上取整"，取代手写循环与
`std::ceil(std::log2(n))`（后者还引入浮点误差与库依赖）。

### 3.2 场景：浮点位级剖析（示例 ③）

`bit_cast<uint32_t>(float)` 后按 IEEE754 拆符号/指数/尾数——
教学、调试 NaN 载荷、浮点哈希的标准起手式。

### 3.3 最佳实践清单

1. 位计数/前导零/取整：`<bit>` 是唯一写法；新代码禁止手写位技巧
2. 类型双关一律 `bit_cast`；`memcpy` 保留给大小不等或非平凡类型
3. 端序判定 `static_assert(std::endian::native == ...)` 写在协议代码
   头部——不支持的平台编译期暴露
4. 位集密度（布隆过滤器、位图统计）用 `popcount`；
   稀疏检测用 `has_single_bit`
5. 有符号数参与位运算前**显式转无符号**——既是 `<bit>` 的要求，
   也是避开符号位陷阱（第 46 课 char 的教训同理）

## 常见陷阱

1. **有符号实参直接传** → 编译错（约束拒绝）；不是静默转换——
   记住"`<bit>` 只收无符号"这一条就避掉九成报错
2. `countl_zero(0)` 返回 32（位数）而不是"没有前导零"或 UB——
   边界值语义专门定义过，别按直觉猜
3. `bit_cast` 两个类型**大小不同** → 编译错；结构体有 padding 时
   padding 位内容不确定——比较/哈希要意识到
4. `bit_ceil` 在类型上限附近溢出（uint32_t 上 `bit_ceil(0x80000001)`
  回绕成小值）——大数先想清楚
5. 循环移位 `x << n | x >> (32-n)` 手写版在 n=0 时右移 32 位是 UB；
   `rotl` 没有这个问题——又一个"标准件救全家"
6. 端序判断写成运行时 `if` 而不是 `static_assert`：不支持的平台
   拖到运行时才炸——协议层要编译期锁死

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
3. 用 `bit_floor/bit_ceil` 实现对齐函数
   `align_down(n)/align_up(n, a)`（a 是 2 的幂，断言之）。
4. `static_assert` 验证：`rotl(rotl(x, 13), 19) == rotl(x, 32)` 对
   任意 uint32_t 成立（32 位取模归零）。

## 参考

- 提案 P0553R4（位操作）/ P0476R2（bit_cast）
- cppreference: [头文件 <bit>](https://zh.cppreference.com/w/cpp/header/bit)
- cppreference: [std::bit_cast](https://zh.cppreference.com/w/cpp/numeric/bit_cast)
