# 第 54 课 · std::generator 与惰性序列（C++23）

> 标准：C++23；本机 MSYS2 GCC 15.1.0 示例链接 `-lstdc++exp`。
> 讲解：本文件；代码：`54_generator.cpp`。
> 前置：第 39 课协程机制、第 33～34 课视图。

## 一、底层原理

### 1.1 标准生成器负责句柄与恢复

`std::generator` 是同步协程生成器。调用生成函数取得一个拥有协程帧的对象，
`begin()` 启动函数体，迭代器递增恢复执行，`co_yield` 把下一个元素交给消费者。
它管理 promise、句柄和销毁，适合逐步产生数据以及递归遍历。

它满足 `view`、`input_range`，不可拷贝、可以移动；不满足 `forward_range`。
可以接 `views::filter/take/transform`，但不能交给要求可排序随机访问范围的 `ranges::sort`。
视图本身也可以保存状态，例如 `iota_view`、带缓存的 `filter_view`；
不能把两者划成“视图无状态、生成器有状态”。生成器的优势是用普通控制流表达跨元素状态。

### 1.2 惰性从函数体开始，不代表调用零成本

生成函数的函数体在首次恢复之前不执行，但调用时仍会准备帧、复制/移动按值参数、
构造 promise；帧分配可能失败，也可能被优化掉。生成器不自动启动线程、异步 I/O 或调度器。
`std::generator` 的 promise 不允许一般的显式 `co_await`；它不是异步任务类型。

### 1.3 elements_of 委托

```cpp
std::generator<const int&> walk(const Node& n) {
    co_yield n.value;                         // n.value 是左值，引用树中的元素
    for (const auto& k : n.kids)
        co_yield std::ranges::elements_of(walk(*k));
}
```

`elements_of` 可以委托兼容的输入范围，包括另一个生成器。
适用条件是范围引用可转换为外层的 `yielded` 类型，不是所有范围都无条件接受。
例如外层 `generator<int>` 的引用类型是 `int&&`，普通 `vector<int>&` 的 `int&`
不能直接转换过去；可以改用外层 `generator<const int&>`，或明确使用移动视图。
第二个参数是分配器，影响通用范围包装协程的帧分配，不用于切换执行上下文。
委托保留递归状态，仍有协程帧和深度成本，不保证任意深度都安全。

## 二、具体语法

```cpp
#include <generator>
#include <ranges>

std::generator<int> small() {
    co_yield 1;
    co_yield 2;
}
for (int x : small() | std::views::take(2)) { /* 使用 x */ }
```

常用类型的区别：

| 类型 | `range_value_t` | `range_reference_t` | 常见用途 |
|---|---|---|---|
| `generator<T>` | `T` | `T&&` | 值序列，也能移出元素 |
| `generator<T&>` | `T` | `T&` | 借用可写对象 |
| `generator<const T&>` | `T` | `const T&` | 借用只读对象 |

`generator<T>` 不等于“每步必定一次拷贝”：yield 左值时可能先复制到挂起期间的存储，
yield 右值与消费者是否按值接收也会影响复制/移动次数。引用产出避免对应的元素复制，
但不免除帧分配和生命周期责任；第二个模板参数还可单独指定值类型。

同一生成器的 `begin()` 只能调用一次，即使前一次只消费一部分。移动后，
已有迭代器指向接收帧的新对象；不要对移出后的对象调用 `begin()`。
多次遍历应重新调用生成函数，或先用 `ranges::to` 保存结果。

## 三、实际应用与最佳实践

示例包含可表示的斐波那契前缀、游程解码与树的深度优先遍历。
斐波那契在加法前检查 `uint64_t` 上界；固定宽度整数不能无界地产生数学整数序列。
`take(n)` 限制消费者取得的数量，不保证源不会为了递增而计算后续元素。

解码格式为重复的“十进制次数 + 一个非数字字符”，例如 `3a2b` 产生 `aaabb`；
空输入和零次数合法，缺次数、末尾只剩数字分别报错，次数溢出抛 `length_error`。
解析在迭代时进行，消费者可能先收到合法前缀，随后才遇到错误。

代码按值接收 `std::string`，把数据保留在帧里。若写成 `const std::string&`，
先 `auto g = rle_decode("2x");` 再迭代时，临时字符串已在声明语句结束时销毁。
直接写在 range-for 初始化表达式中的引用实参临时对象，在符合 C++23 新规则的实现上可能
被延长至循环结束；这一规则不覆盖先前语句中的临时对象，也不挽救被调用函数内已销毁的对象。
按值持有使接口不依赖这些调用位置差异。

树遍历借用 `Node`，消费结束之前树必须存在，且不能进行使所借用对象失效的修改。
帧内局部变量可跨挂起生存，但引用只在对象仍存在时有效；递增、生成器析构或恢复后的
局部变量变化都可能使保存的引用失效。不要让锁跨 `co_yield` 长期占用。

## 常见陷阱

1. 二次 range-for 违反 `begin()` 前置条件，行为未定义；断言是否触发取决于实现与构建配置。
2. 捕获局部变量的协程 lambda 可能依赖已销毁的闭包；优先用按值参数明确保存所需对象。
3. 未迭代时函数体没执行，不能据此断言“调用没有分配、没有构造、不会抛异常”。
4. yield 引用不延长外部对象寿命；保存元素引用之后再恢复协程时要重新检查有效性。
5. 函数体异常可从 `begin()` 或迭代器递增传播，不能只在创建生成器时捕获。
6. 提前退出循环后，帧通常保留到拥有它的生成器析构；析构会销毁局部对象及委托的生成器。

## 完整示例

```bash
g++ -std=c++23 -Wall -Wextra "CPP23/54_std_generator与惰性序列/54_generator.cpp" -o run -lstdc++exp && ./run
```

## 练习

1. 验证解码空输入、`0a2b`、`a`、`12` 与超大次数，区分创建和迭代时抛异常。
2. 用 `generator<const int&>` 实现二叉搜索树中序遍历，明确树的生命周期。
3. 统计 `generator<T>` yield 左值/右值及消费者复制/移动的次数。
4. 阅读二次 `begin()` 的前置条件，保留误用为注释；将结果落地后安全地遍历两次，不运行 UB。

## 参考

- [C++23 草案：generator 类型与成员](https://timsong-cpp.github.io/cppwp/n4950/coro.generator)
- [C++23 草案：yield_value 与委托约束](https://timsong-cpp.github.io/cppwp/n4950/coro.generator.promise)
