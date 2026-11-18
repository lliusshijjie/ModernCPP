# 第 54 课 · std::generator 与惰性序列（C++23）

> 所需标准：**C++23**（`-std=c++23`；本机 g++ 15.1.0 需链接
> `-lstdc++exp`，同第 49 课起全系列）｜ 讲解：本文件 ｜ 独立代码：`54_generator.cpp`
> 前置：第 39 课（协程入门——本课是它手写 Gen 的"官方转正版"）；
> 第 33 课（视图的惰性哲学）

## 一、底层原理

### 1.1 第 39 课的六件套，标准库替你写完了

第 39 课手写生成器要 promise 六件套 + RAII 句柄 + next()——
那是学习协程机制的学费。C++23（P2502，源自 cppcoro）给出
**std::generator**：同样基于协程，但开箱即用：

| | 第 39 课手写 Gen | std::generator |
|---|---|---|
| promise/句柄/析构 | 自己写（约 60 行） | 库内完成 |
| 消费方式 | next()/done() 手动拉 | **range-for / 接视图管道** |
| 产出引用 | 另设成员存值 | `generator<const T&>` 模板参声明 |
| 组合递归 | 手动内层循环 | **`co_yield elements_of(子生成器)`** |

### 1.2 惰性序列的两种实现，终成一家

第 33/34 课的视图是**无状态变换**（filter/transform 只能逐元素改形）；
生成器是**有状态推拉**（递推变量、跨元素记忆住在协程帧里）。
C++23 里两者打通：**generator 本身是 view**——可以直接
`gen | views::take(n) | ...`（示例 ①），"复杂状态源头"与
"轻量加工管道"自由拼接。

### 1.3 elements_of：递归生成的委托语法

树的深度优先遍历要"父节点值 + 各子树序列"拼接——旧协程要手写
内层循环。`co_yield std::ranges::elements_of(sub)` 把子生成器
（或任意区间）的元素**逐个转交**给消费者，递归结构一气呵成
（示例 ③）。标准还允许在委托时**切换分配器/执行上下文**
（`elements_of(sub, alloc)`，进阶不展开）。

## 二、具体语法

```cpp
#include <generator>

std::generator<int> fib() {                 // 无穷序列
    int a = 0, b = 1;
    while (true) {
        co_yield a;                         // 挂起并留值
        auto next = a + b; a = b; b = next;
    }
}

for (int x : fib() | std::views::take(8))   // 生成器接视图管道
    std::print("{} ", x);                   // 0 1 1 2 3 5 8 13

std::generator<const T&> walk(const Node& n) {   // 产出引用版
    co_yield n.value;                       // co_yield 右值 → 存值返回引用
    for (const auto& k : n.kids)
        co_yield std::ranges::elements_of(walk(*k));  // 委托子生成器
}
```

规则细则：

- 默认 `generator<T>` 按值产出；`generator<const T&>` 产出引用
  （不拷贝元素）；`generator<T&&>` 产出可移动引用
- generator 是**输入范围**（input_range）：单趟、只能前进——
  不能 sort/随机访问；多趟消费先落地（第 53 课）
- 移动即转移所有权；拷贝被删除（协程帧唯一持有）
- 生命周期：generator 析构销毁协程帧；迭代中途析构安全
  （第 39 课 RAII 语义的官方版）
- `elements_of` 接受任何区间（容器、视图、另一个 generator）
- 编译期仍是协程：`co_yield` 函数不能是 constexpr/构造/析构
  （第 39 课的限制原样有效）

## 三、实际应用与最佳实践

### 3.1 场景：有状态的序列源（示例 ①/②）

斐波那契、游程编码解码、分页 API 迭代（每页拉取后 yield 一条）——
"序列规则复杂到视图写不出"时的正解：状态住帧里，消费者按需拉。

### 3.2 场景：递归结构遍历（示例 ③）

文件树、JSON、AST 的深度优先展开：每个节点一个 co_yield 值 +
对子节点的 elements_of 委托——递归深度由数据决定，代码无显式栈。

### 3.3 最佳实践清单

1. 无状态变换用视图（第 33 课）、有状态生成用 generator——
   管道左端放 generator、右端放视图
2. 大对象产出用 `generator<const T&>` 免拷贝
3. 递归遍历首选 elements_of，别手写内层 for 拼接
4. generator 单趟：要二次消费就接 `ranges::to` 落地（第 53 课）
5. 协程帧里别锁跨 yield（第 39 课铁律延续）

## 常见陷阱

1. **对同一 generator 二次 range-for**：不是空输出，是**未定义行为**
   ——begin() 的前置条件（尚未启动）已被破坏；libstdc++ 实测直接
   断言中止（Assertion `!_M_began` failed）。再消费 = 重新调用生成
   函数，或落地成容器
2. `generator<T>` 按值存：yield 大对象每步一次拷贝——
   要引用就写 `generator<const T&>`
3. generator 保存的是**协程帧**：返回 generator 的函数里，
   co_yield 引用的局部变量安全（住在帧里）；但**捕获局部容器再
   返回的 lambda** 生成器照旧悬垂（第 33 课视图悬垂的同族问题）
4. elements_of 的子生成器类型必须与外层**产出类型兼容**
  （`generator<const int&>` 委托 `generator<int>` 可以，
   反向引用/值不匹配编译错）
5. 把 generator 传给需要 forward_range 的接口（sort/去重）：
   编译错——它是 input_range，能力边界先看清
6. 忘记惰性：定义了 generator 不消费，任何代码都不会执行——
   "调用了没效果"九成是没迭代

## 完整示例

见 `54_generator.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++23 -Wall -Wextra "CPP23/54_std_generator与惰性序列/54_generator.cpp" -o run -lstdc++exp && ./run
```

## 练习

1. 写 `generator<char> run_length_decode(string_view "3a2b")`，
   与第 34 课视图版对比状态管理方式。
2. 用 `generator<const T&>` 遍历二叉搜索树**中序**（左-根-右），
   验证输出有序——elements_of 的委托顺序即遍历顺序。
3. 把第 39 课练习的 `range(a, b)` 换成 std::generator 实现，
   测量删掉的行数；再对比两者迭代器的类型（input vs 你手写的）。
4. 陷阱复现：对已耗尽的 generator 再 range-for，确认空输出；
   再用 `| ranges::to<std::vector>()` 落地后二次消费。

## 参考

- 提案 P2502R2（std::generator）
- cppreference: [std::generator](https://zh.cppreference.com/w/cpp/utility/generator)
- cppreference: [std::ranges::elements_of](https://zh.cppreference.com/w/cpp/ranges/elements_of)
