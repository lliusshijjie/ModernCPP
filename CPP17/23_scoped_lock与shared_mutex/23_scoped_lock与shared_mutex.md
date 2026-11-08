# 第 23 课 · std::scoped_lock 与 shared_mutex（C++17）

> 所需标准：**C++17**（`<mutex>`/`<shared_mutex>`）｜ 独立代码：`23_scoped_lock.cpp`
> 前置：第 06 课（读写锁）、第 15 课（CTAD——scoped_lock 是 CTAD 旗舰案例）

## 一、底层原理

### 1.1 C++14 留下的两个缺口

第 06 课的 `shared_timed_mutex` 名字里带着"timed"——那是**实现更重**的代价换
超时接口。C++17 补上轻量版 `std::shared_mutex`（纯读写锁，无 timed）。
同时，"一次锁多把锁"的 RAII 工具一直缺位：`lock_guard` 单锁、`unique_lock`
要手动搭配 `std::lock` 算法。`std::scoped_lock` 一站解决——**多锁 + 死锁规避
+ RAII**。

### 1.2 三兄弟对比：scoped_lock 的定位

| | lock_guard | **scoped_lock** | unique_lock |
|---|---|---|---|
| 单锁 RAII | ✓ | ✓ | ✓ |
| **多锁一次锁** | ✗ | **✓** | 需手写 `std::lock` |
| 多锁死锁规避 | — | **✓（使用 std::lock）** | 可配合 std::lock |
| 手动 lock/unlock | ✗ | ✗ | ✓ |
| defer/try/timed | ✗ | ✗ | ✓ |
| 可移动、配条件变量 | ✗ | ✗ | ✓ |
| 开销 | 零 | 零（单锁时等价 lock_guard） | 略重（维护状态位） |

一句话定位：**"到作用域末尾就全解锁"的场景，scoped_lock 是默认答案**；
需要条件变量/延迟加锁/转移所有权才轮到 unique_lock。

### 1.3 死锁是怎么被避开的

经典死锁：线程 1 先锁 A 再锁 B，线程 2 先锁 B 再锁 A——两个都拿着第一把、
等对方的第二把。`scoped_lock(m1, m2)` 内部走 `std::lock` 算法：
使用规避死锁的组合加锁算法，具体调用顺序与退避策略未指定，
不保证随机退避或指数退避。保证针对这次组合加锁；调用前已经持有的其他锁、
重复参数或程序中其他不一致的加锁协议，仍可能导致问题。

**转账是教科书场景**：`transfer(a, b)` 锁 (a.m, b.m)、`transfer(b, a)` 锁
(b.m, a.m)——裸写两个 lock_guard 迟早死锁；scoped_lock 一行终结悬念。

### 1.4 CTAD 旗舰案例

`std::scoped_lock lk(m1, m2);` 靠 CTAD 推出 `scoped_lock<mutex, mutex>`——
没有 CTAD（第 15 课）就得写一长串模板参数。它是标准库给 CTAD 打的
最佳广告。

## 二、具体语法

```cpp
std::mutex m1, m2;
{ std::scoped_lock lk(m1, m2); /* 临界区 */ }   // CTAD，两把一起锁/一起放
{ std::scoped_lock lk(m1); /* ... */ }          // 单锁：等价 lock_guard

std::shared_mutex sm;                            // 轻量读写锁（无 timed）
{ std::shared_lock r(sm);  /* 读 */ }            // 共享
{ std::unique_lock w(sm);  /* 写 */ }            // 独占
```

规则细则：

- scoped_lock **不可拷贝不可移动**，无 `owns_lock`/`unlock`——要精细控制找
  unique_lock
- 多锁构造使用 std::lock，实际获取顺序未指定；析构释放全部锁，不能依赖逆序
- shared_mutex 的使用协议与第 06 课 shared_timed_mutex 完全一致
  （shared_lock 读 / unique_lock 写 / `mutable` 成员配 const 接口），
  只是砍掉了 try_*_for 系列

## 三、实际应用与最佳实践

### 3.1 场景：并发转账（示例 ③-1）

两个账户两把锁、双向并发转账——scoped_lock 保证不死锁，总额守恒可断言。
多对象协同更新的安全底座。

### 3.2 场景：读多写少缓存的轻量版（示例 ③-2）

第 06 课 ConfigCache 换 `shared_mutex`：允许多个读者同时持锁，写者独占。
示例耗时仅供观察，会受到调度与负载影响，不能据此断言正确性或性能优势。

### 3.3 最佳实践清单

1. 默认 `scoped_lock`；要条件变量/延迟/尝试/超时才用 unique_lock
2. 涉及**两把以上锁**的临界区，scoped_lock + `std::lock` 算法是硬规矩
3. 能定全局加锁顺序（永远先锁 A）也可防死锁，但 scoped_lock 让纪律
   变成机制——新人改代码也不会破防
4. 读多写少 + 临界区稍长 → shared_mutex + shared_lock；临界区极短先测
   普通 mutex（读写锁的计数本身有竞争，第 06 课清单重申）
5. scoped_lock 形参传锁**按值列表**，别传数组/容器（不接受）

## 常见陷阱

1. **裸写多把 lock_guard**：不死锁是运气（时序未撞上），上线后流量一高
   就挂——多锁必须 scoped_lock 或 std::lock
2. `scoped_lock lk(m1, m1);` 重复传入同一把非递归锁会违反加锁前提，
   可能死锁或出现未定义行为；组合加锁之前先处理对象别名
3. 以为 scoped_lock 能"中途解锁"——没有 unlock；设计上就要把临界区
   切成更小的块
4. shared_mutex 递归加读锁（同线程两个 shared_lock）= UB（第 06 课同款）
5. 多锁顺序死灰复燃：scoped_lock 规避死锁，但**不减少锁竞争**——
   锁粒度设计仍是你自己的功课

## 完整示例

见 `23_scoped_lock.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织
（③-1 双向并发转账、总额守恒；③-2 观察双读者耗时，不设时间阈值）：

```bash
g++ -std=c++17 -Wall -Wextra "CPP17/23_scoped_lock与shared_mutex/23_scoped_lock.cpp" -o run && ./run
```

## 练习

1. 把示例 ③-1 的 scoped_lock 换成两个 lock_guard 的"裸写版"，调大转账
   次数观察是否偶发死锁（实验完务必还原；卡住就 Ctrl+C）。
2. 用 unique_lock + std::lock 手写等价逻辑，对比 scoped_lock 版的行数
   与可维护性。
3. 三把锁的"三方划账"：A→B、B→C、C→A 并发，scoped_lock 一次锁三把。
4. 给 ③-2 的缓存加写者线程（定时改配置），验证读者始终读到一致快照。

## 参考

- 提案 P0156R2（scoped_lock）/ P0159（shared_mutex）
- cppreference: [std::scoped_lock](https://zh.cppreference.com/w/cpp/thread/scoped_lock)
- cppreference: [std::shared_mutex](https://zh.cppreference.com/w/cpp/thread/shared_mutex)
- 《C++ Concurrency in Action》ch.3 死锁一节

## 补充阅读

[23A · 多对象一致快照与锁边界](../补充知识点/23_补充A_多对象一致快照与锁边界/23_补充A_多对象一致快照与锁边界.md)：组合获取共享锁、处理参数别名，并在锁内复制一致状态。
