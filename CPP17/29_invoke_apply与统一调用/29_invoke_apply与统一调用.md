# 第 29 课 · std::invoke、std::apply 与统一调用（C++17）

> 所需标准：**C++17** ｜ 讲解：本文件 ｜ 独立代码：`29_invoke_apply.cpp`
> 前置：第 03 课（手写 apply_impl——本课让它退休）、第 07 课（decltype(auto) 转发）

## 一、底层原理

### 1.1 INVOKE 协议：从"库内部约定"升格为公开 API

C++11 起标准库内部一直有个 **INVOKE(f, args...)** 协议（`bind`、`function`、
`mem_fn` 都靠它实现）：对四类可调用物按各自"正确的语法"调用。C++17 把它
暴露为 `std::invoke`：

| 可调用物 | 裸调用写法 | invoke 写法 |
|---|---|---|
| 函数 / lambda / 函数对象 | `f(a, b)` | `std::invoke(f, a, b)` |
| 成员函数指针 | `(obj.*pmf)(a)` / `(ptr->*pmf)(a)` | `std::invoke(pmf, obj, a)` |
| **成员数据指针** | `obj.*pmd` | `std::invoke(pmd, obj)`（读/写成员） |

对泛型代码，这是质变：**调用方传来的可能是成员指针**——裸 `f(args...)`
编译不过，`invoke` 统一吃下。

### 1.2 std::apply：tuple 解包调用

`std::apply(f, tup)` = `INVOKE(f, get<0>(tup), get<1>(tup), ...)`——
把 tuple-like 的元素拆开当实参。第 03 课我们用 `index_sequence` 手写了
十几行 `apply_impl`；C++17 一行标准实现，顺带支持 pair/array，
**且内部走 INVOKE 语义**（apply 成员指针 + 对象打包的 tuple 也行）。

### 1.3 组合价值：延迟执行的通用形态

回调/任务队列的经典难题：参数要"存起来，稍后执行"。tuple 打包 + apply
就是标准答案——任何 callable、任何参数组合，统一成一个
`std::function<void()>`。库作者的一大利器。

## 二、具体语法

```cpp
struct Widget { int v; int add(int x) const { return v + x; } };
Widget w{40};
auto pmf = &Widget::add;
auto pmd = &Widget::v;

std::invoke(pmf, w, 2);        // 42 —— 对象是"第一实参"
std::invoke(pmf, &w, 2);       // 42 —— 指针/智能指针/reference_wrapper 都行
std::invoke(pmd, w) = 50;      // 数据成员"调用"返回成员引用：可读可写！

std::apply(add3, std::make_tuple(1, 2, 3));        // add3(1,2,3)
std::apply(&Widget::add, std::make_tuple(w, 2));   // INVOKE 语义：对象来自 tuple
std::apply(add2, std::make_pair(7, 8));            // pair 也可以
```

规则细则：

- invoke 的对象实参接受：对象本身、指针、智能指针、`reference_wrapper`
- `invoke(pmd, w)` 返回的是**成员的引用**（`int&`），不是拷贝
- apply 接受一切 tuple-like（tuple/pair/array）
- invoke/apply 的 constexpr 化在 C++20（第 41 课预告）
- C++23 新增 `std::invoke_r<R>`（强制指定返回类型，第 56 课预告）

## 三、实际应用与最佳实践

### 3.1 场景：完美转发的统一调用层（示例 ③-1）

```cpp
template <class F, class... Args>
decltype(auto) call(F&& f, Args&&... args) {
    return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
}
```
第 07 课的转发 + 本课的 invoke——泛型包装器的"黄金三行"。

### 3.2 场景：延迟任务队列（示例 ③-2）

注册时 `tuple` 打包参数，执行时 `apply` 拆开——任意 callable/参数组合
统一为可存储的闭包。线程池、消息总线、undo 栈同构。

### 3.3 最佳实践清单

1. 泛型代码里调用"用户给的 callable"一律 `std::invoke`（防御成员指针）
2. tuple 解包调用一律 `std::apply`；删掉所有手写 index_sequence 版本
3. 数据成员指针 + invoke = 泛型成员访问器（读/写通吃）
4. 转发 wrapper 返回用 `decltype(auto)`（第 07 课，保值类别）
5. 存储 callable 时 `std::function` 按需特化；C++23 有 move_only_function

## 常见陷阱

1. **invoke 成员指针忘传对象**：`invoke(pmf, 2)` 编译错——第一实参必须是
   对象/指针；看见 "must use .* or ->*" 报错就想到 invoke
2. 成员函数指针直接塞 `std::function<void(int)>` 装不进去——invoke 是
   **调用时**的统一，不是存储层的统一；先 bind/lambda 包一层
3. `invoke(pmd, w)` 拿到引用当拷贝用：改了它就是改成员（本课示例 ①-3
   演示的正是这个能力）
4. apply 硬塞非 tuple-like（vector 等）——编译错；先挪进 tuple
5. make_tuple 会**拷贝**对象；想按引用打包用 `forward_as_tuple`
   （注意其悬空风险，同第 11 课 minmax 教训）

## 完整示例

见 `29_invoke_apply.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++17 -Wall -Wextra "CPP17/29_invoke_apply与统一调用/29_invoke_apply.cpp" -o run && ./run
```

## 练习

1. 用 invoke 写泛型 `get_field(objects, &T::name)`：遍历容器取成员，
   返回 `vector<string>`。
2. 把示例的任务队列改成 `std::deque<std::packaged_task<int()>>`，
   配合 `std::future` 取回结果（第 38 课 jthread 的前菜）。
3. 用 `apply + overloaded` 处理 `variant<tuple<int,int>, tuple<int,int,int>>`：
   两种打包各走一个分支（第 17 课组合技）。
4. 把第 03 课的 `my_apply` 删掉换成 `std::apply`，对比代码量。

## 参考

- 提案 N4480（Library Fundamentals v2，invoke/apply 的出处）
- cppreference: [std::invoke](https://zh.cppreference.com/w/cpp/utility/functional/invoke)
- cppreference: [std::apply](https://zh.cppreference.com/w/cpp/utility/apply)
