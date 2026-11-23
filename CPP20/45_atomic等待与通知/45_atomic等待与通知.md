# 第 45 课 · std::atomic 的等待与通知（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`45_atomic_wait_notify.cpp`
> 前置：第 38 课（jthread/stop_token——本课线程骨架）；第 23 课（锁与同步）

## 一、底层原理

### 1.1 旧世界的两难：自旋 or 条件变量

"等一个原子变量变成某值"常见的两种旧写法：

| 方案 | 代价 |
|---|---|
| 自旋 `while (!flag.load()) ;` | 烧满一个核、延迟抖动（加 sleep 又拖慢响应） |
| `condition_variable` + mutex | 必须配锁与谓词循环；丢唤醒/伪唤醒要人肉防；为一个 int 拖一整套机制 |

C++20 给 `std::atomic` 原生装上 `wait` / `notify_one` / `notify_all`：
实现可先短暂自旋，再使用操作系统等待设施或内部锁/条件变量；
具体机制取决于标准库、平台、类型与大小，标准不保证无锁或一定进入内核。
它把"值仍等于 old 就继续等"的重查循环封装在接口内部。

### 1.2 wait(old) 的精确语义：先查值，再睡觉

```
wait(old) ≈ while (load() 的值表示与 old 相同) 等待并重查
// 此处是语义示意，不是用 operator== 实现，也不限定底层等待机制
```

三个直接推论：

1. **持久状态可抵御提前通知**：先改值后通知，若新值保持到被观察，
   wait 进门看到值已变就直接返回（示例 ①）。
   "通知先于等待"在这类一次性状态协议中不会造成丢唤醒；
   若值改回 old，仍可能错过短暂状态（ABA）
2. **伪唤醒无害**：醒来重查值，值没变接着睡——循环语义内建
3. **比较的是"当下值不等于 old"**：0→5→0 转一圈回来，`wait(0)`
   照样睡——不记账、不计数（示例 ① 的 ABA 注释）

### 1.3 notify 只是"门铃"

- `notify_one/all()` 无等待者 = **no-op**（不排队不缓存，示例 ①）
- 通知不携带数据：数据靠 `store`，通知只负责"摇人"
- 因此铁律：**先改值，后通知**（顺序错就可能错过）

## 二、具体语法

```cpp
std::atomic<int> v{0};
v.wait(0);              // 返回时内部已观察到值表示不同；初次就不同可直接返回
v.notify_one();         // 唤醒一个等待者
v.notify_all();         // 唤醒全部

std::atomic_flag gate{};
gate.test_and_set();    // 置位，返回旧值
gate.clear();           // 清零
gate.wait(true);        // 等"不再是 true"
gate.notify_one();
// C++20：atomic_flag 默认构造即清零态；ATOMIC_FLAG_INIT 宏已废弃

// 自由函数版（参数仍须是 std::atomic<T>*，不能传普通 T*）：
std::atomic_wait(&v, 0);
std::atomic_notify_one(&v);
```

规则细则：

- 比较的是 **值表示**，不是 operator==；永不参与值表示的填充位被忽略。
  atomic<T> 主模板有平凡可拷贝等类型要求，指针/整数等特化也提供此接口
- wait(old, order) 默认 seq_cst，也可显式用 relaxed/acquire 等；
  release/acq_rel 不能用于等待中的 load，违反要求是 UB。
  发布数据通常用 store(..., release) + wait(..., acquire)，notify 自身不发布数据
- atomic_ref<T> 也有 wait/notify；它要求对象对齐及访问方式满足规则，
  不能把普通对象任意与 atomic<T> 互相强转或并发混用非原子访问
- 等待多变量/复杂谓词（"a 或 b"、"求和 > n"）**不适合**：
  回条件变量 + 锁的世界
- 一个对象上同时混用 wait 与锁体系不冲突（各等各的），
  但设计上选一边

## 三、实际应用与最佳实践

### 3.1 场景：数据就绪握手（示例 ③）

生产者先写普通数据，再 `ready.store(true, release); ready.notify_one();`；
消费者 `ready.wait(false, acquire)` 返回后读取数据。只要发布后没有并发修改，
release/acquire 建立的同步关系可保护普通 data，无需它也成为原子。
若反复复用同一缓冲，还需额外的所有权/消费确认协议。

### 3.2 场景：轻量任务关卡（示例 ②）

worker 等待"开工位"翻转：`gate.wait(true)`——主线程 `clear() +
notify_one()` 放行。atomic_flag 的置位/清除等基本原子操作保证无锁，
但大小与等待机制不由此保证；wait 仍可阻塞。

### 3.3 最佳实践清单

1. **单值等待**（标志、计数、游标）首选 atomic wait；
   复杂状态谓词仍用 cv/锁——别硬凑
2. 顺序永远"先改值、后通知"；多处改值共用一个通知点
3. 唤醒后**重读相关变量**再决策（醒来时世界已变——
   对照第 38 课"检查点"思维）
4. 通知不授予数据所有权；多消费者竞取还需 CAS、队列或其他协议。
   notify_one 可以用于多等待者但只需其中一个继续的场景，
   notify_all 适合状态变化需让全部等待者重新检查的场景
5. ABA 敏感场景（指针/序号回环）加版本号或改用带纪元的结构——
   wait 只看当下值

## 常见陷阱

1. **先通知后改值**：等待者醒来重查仍是旧值 → 接着睡；
   你以为通知了，其实等于没通知——顺序写反是最常见事故
2. 以为通知会"攒着"：无等待者的 notify 直接消失（不计数）——
   靠"值先行"兜底，不靠通知排队
3. `wait(old)` 的 old 是**进入时**的期望值：值从 0→5→0，
   `wait(0)` 继续睡——ABA 不是 bug 是语义，但坑住过很多人
4. 把 `wait` 当超时工具用：C++20 的 atomic wait **没有**
   wait_for/wait_until。需要超时优先用条件变量或其他带时限的同步设施；
   只改另一个超时标志再 notify 原对象，原值未变时 wait 仍不会返回
5. wait 返回意味着曾观察到不同于 old 的值表示，但返回值为 void，
   状态也可能再次改变；若业务需要其新值，需 load 重读并按协议判断。
   一次性 ready 标志配合 acquire 时，可以直接读已发布的数据
6. `atomic<double>` 等**按位比较**：`-0.0` 与 `+0.0` 位模式不同，
   wait(-0.0) 可观察到 +0.0 与其不同；已经阻塞时仍需正确通知。
   数值语义敏感处可改用整型标记
7. jthread 的 request_stop 不会自动打断 atomic::wait；
   工作线程若要支持停止，必须设计能改变被等待值并通知的停止协议

## 完整示例

见 `45_atomic_wait_notify.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra -pthread "CPP20/45_atomic等待与通知/45_atomic_wait_notify.cpp" -o run && ./run
```

## 练习

1. 复现"先通知后改值"事故：两个线程故意反序执行，观察等待者
   长睡不醒；再把两行换序验证恢复。
2. 用 `notify_all` + 计数游标写"三线程接力"：atomic<int> 从 0 走到 3，
   每个线程只处理自己的号段。
3. 对比实验：自旋版 `while(load()==0);` 与 `wait` 版的 CPU 占用
   （任务管理器观察），写一段结论。
4. 给第 38 课的"等数据或停止"设计单一原子状态（waiting/ready/stopped）：
   用 CAS 定义数据与停止同时发生时的优先规则，状态变化后 notify_all。
   解释为何依次等待两个 atomic 再加 sleep 无法实现可中断的双条件等待。

## 参考

- [C++20 草案：atomic 等待、通知与 ABA](https://timsong-cpp.github.io/cppwp/n4861/atomics.wait)
- 提案 P1135R6（atomic 的等待与通知）
- cppreference: [std::atomic::wait](https://zh.cppreference.com/w/cpp/atomic/atomic/wait)
- cppreference: [std::atomic_notify_one](https://zh.cppreference.com/w/cpp/atomic/atomic_notify_one)

## 补充阅读

[45A · 原子智能指针与不可变配置快照](../补充知识点/45_补充A_原子智能指针与配置快照/45_补充A_原子智能指针与配置快照.md)。把原子操作应用到整个只读配置的发布，并观察旧版本的生命周期。
