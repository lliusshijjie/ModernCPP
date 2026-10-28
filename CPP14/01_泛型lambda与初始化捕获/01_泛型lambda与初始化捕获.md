# 第 01 课 · 泛型 lambda 与初始化捕获（C++14）

> 所需标准：C++14 ｜ 讲解：本文件 ｜ 独立代码：`01_generic_lambda.cpp`

## 一、底层原理

### 1.1 lambda 不是函数，是"编译器生成的匿名类对象"

每个 lambda 表达式都会让编译器生成一个独一无二的**闭包类型**（匿名类），
并创建该类型的对象。写法与生成物一一对应：

| lambda 写法 | 编译器生成物 |
|---|---|
| 捕获列表 `[a, b]` | 匿名类的**成员变量** |
| 函数体 `{...}` | **成员函数** `operator()` |
| 默认（非 mutable） | `operator()` 是 **const** → 不能修改捕获的成员 |
| 形参 `auto`（C++14 新增） | `operator()` 升级为**成员模板** |

所以"泛型 lambda"的本质就一句话：把 C++11 的普通成员函数 `operator()`
换成了**函数模板**，调用点按实参实例化。与编译器为 `add` 生成物等价的手写类：

```cpp
struct Adder {
    template <class A, class B>
    auto operator()(A a, B b) const { return a + b; }
};
```

从这张对应表能推出本课所有行为：

- 无捕获的 lambda 可隐式转换为匹配的普通函数指针；其闭包大小不由标准固定为 1
- **每条** lambda 表达式的类型都独一无二——函数体一模一样的两条也不相同
- 按值捕获需要保存相应状态，但闭包布局、填充和最终大小属于实现细节；
  示例中的大小只能作为当前编译器上的观察结果

### 1.2 初始化捕获的本质：捕获列表是"成员初始化列表"的语法糖

`[total = 0]` 声明一个名为 `total` 的新成员并用表达式 `0` 初始化；
`[p = std::move(payload)]` 声明成员 `p` 并**移动初始化**——这就是"移动捕获"。

为什么需要新语法？C++11 的普通按值捕获会从现有变量拷贝，
无法直接把不可拷贝的 `std::unique_ptr` 移入闭包并转移所有权。
按引用捕获虽可访问该指针，却不接管资源，也不延长其生命周期。
C++14 允许显式指定捕获的初始化表达式，使独占资源的移动捕获可以直接表达。

### 1.3 返回类型推导的放宽与缺陷修正

最初发布的 C++11 对 lambda 返回类型推导限制较多。C++14 将其统一为与普通函数
相同的返回类型推导规则，多个 return 分支须推导出一致类型。
该调整也作为缺陷修正追溯到 C++11，所以现代编译器在 C++11 模式下也可能接受相关写法。

## 二、具体语法

### 2.1 泛型 lambda：auto 形参

```cpp
auto add = [](auto a, auto b) { return a + b; };

add(2, 3);                        // 5      → 实例化 operator()(int, int)
add(1.5, 2);                      // 3.5    → 混型也行
add(std::string("mo"), "dern");   // modern → 第三次实例化
```

规则细则：

- 每个 `auto` 形参是**独立**的模板参数
- `auto&&` 是转发引用；`auto` 按值传参，实参不同可能发生拷贝、移动或相应省略
- 各 `return` 分支的类型必须能推导一致，否则编译错误
- C++14 还没有语言级 Concepts（第 31 课），可以使用 SFINAE 等模板技术约束接口，见补充 03A

### 2.2 初始化捕获（init-capture）

```cpp
[total = 0]              // 新成员 total，初值 0
[p = std::move(ptr)]     // 新成员 p，移动而来 —— "移动捕获"
[&r = get_ref()]         // 引用成员（少用，注意悬空）
```

规则细则：

- 表达式在 **lambda 创建时**求值一次，不是每次调用时
- 修改按值捕获的成员必须加 `mutable`（去掉 `operator()` 的 const）
- 初始化捕获引入的名字只存在于闭包内，不要求外部有同名变量

### 2.3 多 return 自动推导

```cpp
auto clamp01 = [](double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;            // C++14 支持；现代 C++11 模式也可能通过追溯缺陷修正支持
};
```

## 三、实际应用与最佳实践

### 3.1 场景：一个谓词吃遍所有容器

```cpp
auto desc = [](auto a, auto b) { return a > b; };
std::sort(vi.begin(), vi.end(), desc);   // vector<int>
std::sort(vd.begin(), vd.end(), desc);   // vector<double> —— 同一个闭包复用
```

C++11 要么写两份 lambda，要么抽模板函数；泛型 lambda 就地解决。

### 3.2 场景：把不可拷贝资源交给"延迟任务"

socket、文件句柄、`unique_ptr` 等资源需要转移给延迟工作时，
C++14 移动捕获提供了直接表达独占所有权转移的语法（示例 ③-2）。

**但有一个边界**：含 `unique_ptr` 的闭包是**只可移动**的，而 `std::function`
要求其目标可拷贝——所以不能直接装入以 `std::function` 为元素的任务队列。
它可以直接调用、作为线程入口，或装入支持移动的任务包装，例如 `std::packaged_task`。
只有确实需要共享所有权时才改用 `shared_ptr`；C++23 的 `std::move_only_function`
（第 56 课）还提供了专门的只移动函数包装器。

### 3.3 最佳实践清单

1. **只读不复制**：泛型 lambda 的只读参数写 `auto&&`，避免 `auto` 隐式深拷贝大对象
2. **需要完美转发**：`[](auto&&... xs) { return f(std::forward<decltype(xs)>(xs)...); }`
3. **捕获方式选择**：大对象只读 → 按引用 `[&x]`；要延长生命周期/跨线程 → 按值或移动捕获
4. 移动捕获后按类型契约使用源对象：`unique_ptr` 移动构造后为空，
   `vector` 等标准库对象通常有效但值未指定，可以重新赋值，不能统一假定为空
5. **有状态 lambda（如计数器）**：拷贝 lambda = 拷贝状态，两个副本各自计数，互不干扰

## 常见陷阱

1. 初始化捕获只在创建时求值一次。`[v = expensive_call()]` 只调用一次；
   要"每次调用取最新值"应按引用捕获 `[&v]`
2. `mutable` 修改的是闭包成员的**副本**——拷贝 lambda 会连状态一起拷
3. 本课的 `unique_ptr` 移动捕获后源指针为空，不能再解引用；不要把它推广到所有类型
4. `auto` 形参按值传大对象造成隐性拷贝；热路径上用 `auto&&`
5. 含 `unique_ptr` 移动捕获的闭包**只可移动**，装不进 `std::function`
   （它要求目标可拷贝）——任务队列也可以选择支持移动的任务包装，不能因此强制共享所有权

## 完整示例

见 `01_generic_lambda.cpp`，按 **① 原理验证 → ② 语法演示 → ③ 应用实战** 三段组织，
先预测输出再运行（示例输出刻意只用 ASCII，规避 Windows 控制台编码问题）：

```bash
g++ -std=c++14 -Wall -Wextra "CPP14/01_泛型lambda与初始化捕获/01_generic_lambda.cpp" -o run && ./run
```

## 练习

1. 不运行示例，先手写出 `auto add = [](auto a, auto b) { return a + b; };`
   的编译器等价类；再用 `static_assert` + `std::is_same` 验证：
   两条函数体相同的 lambda 类型确实不同。
2. 用初始化捕获造一个"调用计数器" lambda：每次调用返回自己是第几次被调用；
   拷贝一份该 lambda，验证副本计数独立。
3. 用移动捕获把装满 `int` 的 `vector` 搬进 lambda，lambda 返回元素和，
   观察源 vector 的状态，再给它重新赋值；不要把观察到的空状态当成通用保证。
4. 写一个只读参数用 `auto` 的泛型 lambda 接收 10 万元素的 `string`，
   改成 `auto&&` 后用性能对比（或反汇编）确认拷贝消失了。

## 参考

- cppreference: [Lambda 表达式](https://zh.cppreference.com/w/cpp/language/lambda)
- 《Effective Modern C++》Item 31（避免默认捕获模式）、Item 32（用初始化捕获把对象移入闭包）
