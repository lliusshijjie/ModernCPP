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

- 无捕获的 lambda 是空类 → `sizeof == 1`（零开销抽象），
  且可隐式转换为普通函数指针（调用不需要 `this`）
- **每条** lambda 表达式的类型都独一无二——函数体一模一样的两条也不相同
- 捕获几个变量，闭包对象就多大：`[extra]` 的闭包 `sizeof == sizeof(int)`

### 1.2 初始化捕获的本质：捕获列表是"成员初始化列表"的语法糖

`[total = 0]` 声明一个名为 `total` 的新成员并用表达式 `0` 初始化；
`[p = std::move(payload)]` 声明成员 `p` 并**移动初始化**——这就是"移动捕获"。

为什么 C++11 做不到？C++11 的捕获只有两种机制：按值（**拷贝**成成员）、
按引用（**绑定**成成员）。`std::unique_ptr` 不可拷贝，于是怎么都进不了闭包——
不是缺语法，是"成员只能从已存在的变量拷贝/绑定"这条底层规则堵死了路。
C++14 把"成员如何初始化"直接开放为任意表达式，问题从根上消失。

### 1.3 返回类型推导为什么 C++11 只许一条 return

lambda 的返回类型走 `auto` 推导。C++11 规定：仅当函数体是"单条 return"才推导，
否则必须显式 `-> T`。C++14 放开为与普通函数相同的返回类型推导规则
（多个 return 分支必须推导出一致类型）。

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
- `auto&&` 是转发引用，`auto` 则总是拷贝按值传入
- 各 `return` 分支的类型必须能推导一致，否则编译错误
- C++14 还没有约束手段（concepts 是 C++20，第 21 课），传错类型要到实例化点才报错

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
    return x;            // C++11: 编译错误；C++14: OK
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

socket、文件句柄、`unique_ptr` 想移入回调/任务队列/线程入口，
C++11 无解（拷不动），C++14 移动捕获是教科书用法（示例 ③-2）。

**但有一个边界**：含 `unique_ptr` 的闭包是**只可移动**的，而 `std::function`
要求其目标可拷贝——所以这种闭包进不了任务队列（示例代码注释里就是编译报错现场）。
它适合**直接调用**或做**线程入口**（`std::thread` 的 callable 可移动即可）；
非要入队，所有权改用 `shared_ptr`；C++23 的 `std::move_only_function`
（第 37 课）才是只移动闭包的正式归宿。

### 3.3 最佳实践清单

1. **只读不复制**：泛型 lambda 的只读参数写 `auto&&`，避免 `auto` 隐式深拷贝大对象
2. **需要完美转发**：`[](auto&&... xs) { return f(std::forward<decltype(xs)>(xs)...); }`
3. **捕获方式选择**：大对象只读 → 按引用 `[&x]`；要延长生命周期/跨线程 → 按值或移动捕获
4. **移动捕获后原变量立即失效**，紧随其后的代码别再碰它
5. **有状态 lambda（如计数器）**：拷贝 lambda = 拷贝状态，两个副本各自计数，互不干扰

## 常见陷阱

1. 初始化捕获只在创建时求值一次。`[v = expensive_call()]` 只调用一次；
   要"每次调用取最新值"应按引用捕获 `[&v]`
2. `mutable` 修改的是闭包成员的**副本**——拷贝 lambda 会连状态一起拷
3. 移动捕获后原指针为 `nullptr`，再解引用是空指针解引用
4. `auto` 形参按值传大对象造成隐性拷贝；热路径上用 `auto&&`
5. 含 `unique_ptr` 移动捕获的闭包**只可移动**，装不进 `std::function`
   （它要求目标可拷贝）——线程入口可以直接用，任务队列请换 `shared_ptr`

## 完整示例

见 `01_generic_lambda.cpp`，按 **① 原理验证 → ② 语法演示 → ③ 应用实战** 三段组织，
先预测输出再运行（示例输出刻意只用 ASCII，规避 Windows 控制台编码问题）：

```bash
g++ -std=c++14 -Wall -Wextra lessons/01_generic_lambda.cpp -o run && ./run
```

## 练习

1. 不运行示例，先手写出 `auto add = [](auto a, auto b) { return a + b; };`
   的编译器等价类；再用 `static_assert` + `std::is_same` 验证：
   两条函数体相同的 lambda 类型确实不同。
2. 用初始化捕获造一个"调用计数器" lambda：每次调用返回自己是第几次被调用；
   拷贝一份该 lambda，验证副本计数独立。
3. 用移动捕获把装满 `int` 的 `vector` 搬进 lambda，lambda 返回元素和，
   并验证原 vector 移动后为空。
4. 写一个只读参数用 `auto` 的泛型 lambda 接收 10 万元素的 `string`，
   改成 `auto&&` 后用性能对比（或反汇编）确认拷贝消失了。

## 参考

- cppreference: [Lambda 表达式](https://zh.cppreference.com/w/cpp/language/lambda)
- 《Effective Modern C++》Item 31（避免默认捕获模式）、Item 32（用初始化捕获把对象移入闭包）
