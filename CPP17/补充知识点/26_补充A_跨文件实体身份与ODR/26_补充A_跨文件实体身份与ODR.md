# 补充 26A · 跨文件实体身份与 ODR（C++17）

> 阅读顺序为第 26 课之后，不占每日主线编号。
> 所需标准为 C++17。本节使用一个头文件与两个独立翻译单元。
> 主程序：[26a_odr_identity.cpp](26a_odr_identity.cpp)；
> 第二翻译单元：[26a_odr_peer.cpp](26a_odr_peer.cpp)；
> 共享头文件：[26a_shared.hpp](26a_shared.hpp)。

## 一、底层原理

### 1.1 为什么一个 cpp 里的地址比较不够？

在同一个翻译单元内，把变量绑到两个引用上再比较地址，
只能证明这两个引用指向同一个对象。
要验证头文件被多个 cpp 包含后的实体身份，需要真的分别编译两个翻译单元。

本例的 main 与 peer 各自包含同一个头文件。
peer 返回它看到的地址，main 再与自己看到的地址比较。
不打印受系统地址布局影响的具体数值，而是打印地址是否相等。

| 头文件定义 | 本例跨 TU 的身份 |
|---|---|
| `inline int shared_calls = 0;` | 外部链接的同一个实体 |
| `static int local_calls = 0;` | 每个 TU 各有一个对象 |
| `inline constexpr int protocol = 17;` | 同一个实体、同一个地址 |
| `constexpr int local_tag = 17;` | 此处为内部链接，每个 TU 各一份 |
| 类内 `static constexpr int attempts = 3;` | C++17 隐式 inline，同一个实体 |

这里讨论普通命名空间作用域变量，没有匿名命名空间、额外 static、
thread_local 等改变链接或存储规则的修饰。

### 1.2 inline 允许跨 TU 定义，不允许任意重复定义

同一个翻译单元内仍不能把同一个变量定义两遍。
头文件保护用于避免一个 TU 内重复包含；inline 解决的是多个 TU 的定义问题，
两者作用不同。

跨 TU 的多重定义必须满足 ODR 条件。定义具有相同 token 序列只是其中一项，
其中名称查找的结果也需要符合相同实体等规则。
用宏让不同文件看到不同初始值，可能违反 ODR，而且通常不要求诊断；
“链接成功”不能作为定义一致的证明。

### 1.3 头文件里的内部链接对象还有一个细节

假设头文件中有：

```cpp
constexpr int local_tag = 17;
// 反例说明，配套程序没有定义下面这个函数：
inline const int* bad_address() { return &local_tag; }
```

跨 TU 的 inline 函数定义中，取地址会 odr-use 各自不同的 local_tag。
虽然函数文本看起来一样，却会违反相关 ODR 条件。
本例在各 TU 自己的普通函数中取地址，不通过这种跨 TU inline 函数暴露内部对象。

相反，只读取值而不 odr-use、并满足同值常量等标准例外条件时，可以引用不同 TU
的内部链接常量。初学时不必依赖这个例外设计接口；
需要共享身份的常量直接用 inline constexpr 更清楚。

## 二、具体语法

### 2.1 头文件、声明与定义如何分工？

`26a_shared.hpp` 放变量、类型和 inline registry() 的定义，
同时声明 peer_addresses() 与 peer_visit()。
两个普通 peer 函数仅在 peer.cpp 中定义，主程序通过声明调用它们。

不能在主程序中 `#include "26a_odr_peer.cpp"`。
那会把文件文本拼进一个翻译单元，失去本节想验证的编译边界。

### 2.2 外部链接 inline 函数的局部 static 也共享

```cpp
inline Registry& registry() {
    static Registry instance;
    return instance;
}
```

这里的 registry 函数具有外部链接。
它的局部 static 在不同 TU 中指向同一个对象，适合首次使用时构造的共享服务。
若把函数本身改为内部链接，就不再得到这个跨 TU 共享保证。

函数局部 static 的首次动态初始化有同步保证；
初始化完成之后，对 visits 的普通自增不会自动变成线程安全操作。
本例在一个线程中依次调用，需要并发更新时仍要使用锁或原子操作。

### 2.3 动态初始化与实体唯一要分开理解

inline 解决实体可在头文件中定义的问题，不表示所有初始化依赖都自动有序。
普通非模板 inline 变量的动态初始化属于部分有序初始化：
定义在各 TU 中保持一致先后等条件，可以建立初始化次序；
不满足相关条件时，不能随意假定顺序。

有初始化依赖时，可以优先使用能静态初始化的 inline constexpr，
或者把依赖显式放到函数内，按需要构造。
这也不等于可以忽略递归初始化、销毁顺序或后续共享状态的同步。

## 三、实际应用与最佳实践

主程序和 peer 各执行一次访问：

- shared_calls 最终为 2，表明共享同一份状态。
- main 的 local_calls 为 1，peer 的 local_calls 也为 1，表明状态分开。
- registry 地址相同，其 visits 最终为 2。

这适合理解头文件库的版本标识、共享计数器和按需构造的注册表。
选择 inline、内部链接或普通 extern 声明，应由“是否需要共享实体”决定，
不能仅以是否避开了链接错误为标准。

## 常见陷阱

- 在头文件用 static 消除重定义错误，却意外把全局状态变成每个 TU 一份。
- 两个 cpp 分别复制一段 inline 定义，再通过宏或修改让它们不同。
- 用跨 TU inline 函数取内部链接对象的地址，忽略名称查找造成的 ODR 问题。
- 把 pragma once 或 include guard 当成跨 TU 的去重机制。
- 把 inline 当作并发同步或任意初始化顺序的保证。

## 完整示例

在仓库根目录将两个 cpp 一起交给编译器；编译器会分别处理，再链接：

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic "CPP17/补充知识点/26_补充A_跨文件实体身份与ODR/26a_odr_identity.cpp" "CPP17/补充知识点/26_补充A_跨文件实体身份与ODR/26a_odr_peer.cpp" -o run.exe
.\run.exe
```

地址比较依次为 true、false、true、false、true。
随后输出 `shared/local/peer-local: 2 1 1`、
`registry same: true visits=2`，最后为 `checks: PASS`。

只编译主程序会缺少 peer 函数的定义，预期在链接阶段失败。
也可以先用 -c 分别生成两个对象文件，再交换链接时的顺序；
结果不应依赖先列出哪一个对象文件。

## 练习

1. 将 shared_calls 改为 static int，预测哪些身份和状态检查不再通过。
2. 将 local_tag 改为 inline constexpr，预测地址比较的变化，并调整检查。
3. 把 registry 改为具有内部链接的函数，比较 visits 与地址。
4. 若要在两个线程中同时执行 peer_visit，列出需要同步保护的状态。

## 参考

- [C++17 草案 N4659 · inline 与跨 TU 地址](https://timsong-cpp.github.io/cppwp/n4659/dcl.inline)
- [C++17 草案 N4659 · ODR 与内部链接常量例外](https://timsong-cpp.github.io/cppwp/n4659/basic.def.odr)
- [C++17 草案 N4659 · 非局部变量的动态初始化](https://timsong-cpp.github.io/cppwp/n4659/basic.start.dynamic)

## 学习衔接

从 [第 26 课](../../26_inline变量与ODR的现代化/26_inline变量与ODR的现代化.md) 进入本节，
其他内容见 [C++17 补充知识点目录](../README.md)。
