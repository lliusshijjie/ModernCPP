# 第 38 课 · std::jthread 与 std::stop_token（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`38_jthread_stop.cpp`
> 前置：第 23 课（mutex/scoped_lock——线程同步基础）；第 12 课（RAII——jthread 的灵魂）

## 一、底层原理

### 1.1 旧线程的两宗罪

| 罪 | `std::thread` 的行为 | `std::jthread` 的行为 |
|---|---|---|
| 忘 join/忘 detach | 析构时 **`std::terminate`** 直接崩 | 析构 = `request_stop()` + `join()`（RAII） |
| 取消协作无标准件 | 手写 `std::atomic<bool> stop` 各搞各的 | 内置 **stop 状态机**（source/token/callback 三件套） |

jthread = **joining thread**：第 12 课的 RAII 思想终于落到线程上——
"线程生命期绑定对象生命期，离开作用域前必然干净收尾"。

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
- 若线程入口函数**第一个参数是 `std::stop_token`**，jthread 自动把自己的
  token 塞进去（"with stop token" 入口）——不需要额外接线
- `request_stop()` 是**协作式**的：只置位不抢杀；工作线程必须在循环/等待处
  **主动检查** `stop_requested()`

### 1.3 stop_callback 的两个关键语义

1. 回调在**调用 `request_stop()` 的线程**里同步执行（不在工作线程）——
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
cv.wait(lock, tk, pred);   // 收到 stop 就醒来（wait 返回 false 表示被停）
```

规则细则：

- `stop_possible()`：token 有没有可能被停（空头 token 为 false）
- 同一 stop 状态可接**多个** callback；callback 对象析构即注销
- jthread 不可拷贝，可移动；移动后的旧对象 `joinable() == false`
- `detach()` 仍可用，但 detach 后析构不再 join——stop 状态仍共享，
  token 继续有效（停止权在握，join 义务已放）
- 等待 + stop 的组合首选 `condition_variable_any::wait(lock, token, pred)`
  ——普通 `condition_variable` 没有 token 重载（它要求 unique_lock，
  `_any` 才是通用版）

## 三、实际应用与最佳实践

### 3.1 场景：可取消的后台任务（示例 ②/③）

循环体第一行检查 `st.stop_requested()`；阻塞等待处用
`cv.wait(lock, st, pred)`——两处都是"检查点"，取消延迟以检查点为界。
jthread 析构自动收尾，main 提前 return / 异常展开都安全。

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
   但**依赖析构收尾等于接受最晚退出时机**；需要"完成后立刻继续"就显式 join
5. `std::thread` 混用旧习：`std::thread t(...)` 忘 join 析构直接 terminate；
   迁移清单 = 换类型 + 首参加 token + 循环加检查点
6. 二次 `request_stop()` 返回 false——用返回值区分"我首发"还是"别人已停"

## 完整示例

见 `38_jthread_stop.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra "CPP20/38_jthread与stop_token/38_jthread_stop.cpp" -o run && ./run
```

## 练习

1. 把一个自旋 worker（`while(!stop) 累加`）改成每轮 `sleep_for(1ms)`，
   观察取消延迟的变化，并解释"检查点密度决定取消延迟"。
2. 用 `stop_source` 同时停两个 jthread（共享同一 token），验证一处请求、
   双双退出。
3. 复现陷阱 3：先 `request_stop()` 再注册 callback，打印回调执行时机
   （应在注册线程立即执行）。
4. 用 `condition_variable_any::wait(lock, token, pred)` 写"等数据或停止"：
   主线程 10ms 后塞数据，worker 醒来处理后再被停——输出两条不同路径。

## 参考

- 提案 P0660R10（stop_token/jthread）
- cppreference: [std::jthread](https://zh.cppreference.com/w/cpp/thread/jthread)
- cppreference: [std::stop_callback](https://zh.cppreference.com/w/cpp/thread/stop_callback)
