# 第 38 课 · std::jthread 与 std::stop_token（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`38_jthread_stop.cpp`
> 前置：第 23 课（mutex/scoped_lock——线程同步基础）；第 12 课（RAII——jthread 的灵魂）

## 一、底层原理

### 1.1 旧线程的两宗罪

| 罪 | `std::thread` 的行为 | `std::jthread` 的行为 |
|---|---|---|
| 忘 join/忘 detach | 析构时 **`std::terminate`** 直接崩 | 析构时若 joinable，则 `request_stop()` + `join()`（RAII） |
| 取消协作无标准件 | 手写 `std::atomic<bool> stop` 各搞各的 | 内置 **stop 状态机**（source/token/callback 三件套） |

jthread = **joining thread**：第 12 课的 RAII 思想终于落到线程上——
通常可让作用域退出时请求停止并等待线程，但工作线程必须配合退出，
依赖的资源也须活到 join 完成；`detach`、自连接或锁依赖等仍需谨慎处理。

### 1.2 stop 三件套：source / token / callback

```
stop_source ──拥有──> stop 状态（引用计数共享）
    │ request_stop()：发出停止信号（返回 bool：是否首次）
    ▼
stop_token ──只读视图──> stop 状态
    │ stop_requested()：有没有人要求停
    ▼
stop_callback<C>：注册"停时回调"，随对象注销而解除
```

- jthread **自带一个 stop_source**：`get_stop_source()` / `get_stop_token()`
- 若入口可调用对象能以 `stop_token` 加其余实参调用，jthread 优先传入自己的
  token；否则尝试不带 token 的调用。首参写 `std::stop_token` 是常用形式，
  泛型可调用对象也可以接收 token
- `request_stop()` 是**协作式**的：只置位不抢杀；工作线程必须在循环/等待处
  **主动检查** `stop_requested()`；请求方还会同步执行已注册的停止回调

### 1.3 stop_callback 的两个关键语义

1. 回调在**成功发出停止请求的线程**里同步执行，该线程也可能就是 worker——
   回调里别做长活，它阻塞请求方
2. 注册时若**已停止**，回调**立即在注册线程执行**——天然处理"迟到注册"

## 二、具体语法

```cpp
std::jthread t([](std::stop_token st) {        // 首参 stop_token：自动接线
    while (!st.stop_requested()) { /* 干活 */ }
});
t.request_stop();          // 或 t.get_stop_source().request_stop()
t.get_stop_token().stop_requested();
{ std::jthread t2{work}; }                        // 作用域尾：自动 stop + join

std::stop_source ss;                              // 也可脱离 jthread 单独用
std::stop_token tk = ss.get_token();
std::stop_callback cb{tk, [] { /* 停时回调 */ }};
bool first = ss.request_stop();                   // 首次 true，之后 false

// 与条件变量联动：可被 stop 打断的等待（免自旋）
std::condition_variable_any cv;
bool has_data = cv.wait(lock, tk, pred); // 返回最终 pred() 的值；stop 与数据可同时发生
```

规则细则：

- token 的 `stop_possible()`：已请求停止，或关联状态仍有 stop_source 可发请求。
  无关联状态为 false；所有 source 消失且尚未停止时也为 false
- 同一 stop 状态可接**多个** callback；callback 对象析构会注销，
  必要时等待另一线程正在执行的回调结束；回调若抛异常会 terminate
- jthread 不可拷贝，可移动；移动后的旧对象 `joinable() == false`
- `detach()` 后不再 join，析构也不再自动请求 stop。持有的 token 仍引用状态，
  但 token 本身不能发请求；若需之后停止，须保留可用的 stop_source
- 等待 + stop 的组合首选 `condition_variable_any::wait(lock, token, pred)`
  ——普通 `condition_variable` 没有 token 重载（它要求 unique_lock，
  `_any` 才是通用版）

## 三、实际应用与最佳实践

### 3.1 场景：可取消的后台任务（示例 ②/③）

循环体第一行检查 `st.stop_requested()`；阻塞等待处用
`cv.wait(lock, st, pred)`——两处都是"检查点"，取消延迟以检查点为界。
jthread 析构在 joinable 时自动 stop + join；锁和共享数据须安排好析构顺序，
不能在持有 worker 所需锁时阻塞 join。入口函数中未捕获异常仍会 terminate。

### 3.2 场景：跨组件的停止信号（示例 ①）

`stop_source` 可以脱离线程存在：配置模块持有 source、十个消费者各持
token——一处 `request_stop()`，全局收工。比广播 atomic<bool> 干净得多。

### 3.3 最佳实践清单

1. 新代码一律 jthread；`std::thread` 只在"绝不取消且手动管理"时考虑
2. 工作函数首参写 `std::stop_token`，让接线自动化
3. 每个循环与每个阻塞等待都是**取消检查点**——没有检查点的协作取消
   等于没有取消
4. stop_callback 里只做短平快（置标志、推事件），长活交给线程自己
5. 停止后要清理资源（关文件、释放句柄）→ 写在线程内、`stop_requested()`
   分支后，而不是 stop_callback 里

## 常见陷阱

1. `request_stop()` 不是 kill：工作线程不检查 token 就永远不退——
   "调了 stop 程序还卡着"九成是缺检查点
2. stop_callback 在**请求方线程**同步跑：回调里再锁请求方持有的锁 = 死锁
3. 已停止后再注册 callback：立即执行（不是永远不执行）——时序敏感处
   要意识到这条
4. jthread 忘记 join 就让它离开作用域**不是**错误（自动 stop+join），
   但析构可能阻塞；显式 `join()` 只等待、不发停止请求。循环任务通常要先
   `request_stop()` 再 `join()`，让任务自然完成时则直接 join
5. `std::thread` 混用旧习：`std::thread t(...)` 忘 join 析构直接 terminate；
   迁移清单 = 换类型 + 首参加 token + 循环加检查点
6. 二次 `request_stop()` 返回 false——用返回值区分"我首发"还是"别人已停"

## 完整示例

见 `38_jthread_stop.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra -pthread "CPP20/38_jthread与stop_token/38_jthread_stop.cpp" -o run && ./run
```

## 练习

1. 把一个自旋 worker（`while(!stop) 累加`）改成每轮 `sleep_for(1ms)`，
   观察取消延迟的变化，并解释检查点间的工作/阻塞与调度如何影响延迟。
   `sleep_for` 不会被 stop_token 自动中断。
2. 用独立的 `stop_source` 同时停两个 worker：让线程入口捕获共享 token，
   明确忽略或不接收 jthread 自动注入的 token。验证外部 source 停止两线程；
   此时各 jthread 自己的 request_stop() 不会停止外部 token 对应的状态。
3. 复现陷阱 3：先 `request_stop()` 再注册 callback，打印回调执行时机
   （应在注册线程立即执行）。
4. 用 `condition_variable_any::wait(lock, token, pred)` 写"等数据或停止"：
   主线程 10ms 后塞数据，worker 醒来处理后再被停——输出两条不同路径。

## 参考

- [C++20 草案：可中断等待的返回值](https://timsong-cpp.github.io/cppwp/n4861/thread.condvarany.intwait)
- [C++20 草案：jthread 构造、析构](https://timsong-cpp.github.io/cppwp/n4861/thread.jthread.cons)
- 提案 P0660R10（stop_token/jthread）
- cppreference: [std::jthread](https://zh.cppreference.com/w/cpp/thread/jthread)
- cppreference: [std::stop_callback](https://zh.cppreference.com/w/cpp/thread/stop_callback)
