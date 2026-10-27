# 第 06 课 · std::shared_timed_mutex 与读写锁（C++14）

> 所需标准：C++14（`-pthread` 线程库）｜ 讲解：本文件 ｜ 独立代码：`06_shared_timed_mutex.cpp`

## 一、底层原理

### 1.1 读多写少的场景，普通 mutex 亏在哪

4 个线程**只读**同一份配置，`std::mutex` 也让它们排队——并行度白白丢掉。
C++11 只有两个选择：忍着，或用 `condition_variable` 手搓读写锁（几十行、
极易出错）。C++14 把 `std::shared_timed_mutex` + `std::shared_lock` 标准化；
C++17 又补了一个轻量版 `std::shared_mutex`（无 timed 接口，第 23 课）。

### 1.2 读写锁的状态机（实现视角）

锁内部维护"读者计数 + 写者标志"：

| 当前状态 | `lock_shared`（读） | `lock`（写） |
|---|---|---|
| 空闲 | ✓ 立即获得 | ✓ 立即获得 |
| 已有 N 个读者 | ✓ 计数 +1 并入 | ✗ 等读者清零 |
| 写者持有 | ✗ | ✗ |

两个关键细节：

- **防写者饿死**：libstdc++ 的实现会给"等待中的写者"记账，有写者在等时，
  新读者也会排队。但**标准不规定优先级策略**——读者优先/写者优先因实现而异
- **同步语义**与普通锁一致：写者 unlock（或最后一个读者 unlock_shared）是
  release，之后任何 lock/lock_shared 是 acquire——读者必然看到写者的全部变更

### 1.3 名字里的 "timed" 是什么代价

比 C++17 的 `shared_mutex` 多一整套 `try_*_for / try_*_until` 超时接口，
用途是超时退避与死锁诊断。代价是实现更重（内部多一层超时状态）。
不需要超时就用轻量的 `shared_mutex`。

## 二、具体语法

### 2.1 成员接口表

| | 独占（写） | 共享（读） |
|---|---|---|
| 阻塞加锁 | `lock()` | `lock_shared()` |
| 非阻塞尝试 | `try_lock()` | `try_lock_shared()` |
| 超时尝试 | `try_lock_for/until()` | `try_lock_shared_for/until()` |
| 解锁 | `unlock()` | `unlock_shared()` |

### 2.2 RAII 配套（重点！）

- **写侧**：`std::unique_lock` 或 `std::lock_guard`
- **读侧**：只能用 `std::shared_lock`——`lock_guard` 的构造只会调 `lock()`，
  那是**独占**锁，用它在读路径上等于退回普通 mutex
- `shared_lock` 的修饰词与 `unique_lock` 同款：`defer_lock` / `try_to_lock` /
  `adopt_lock`，以及 `lock/unlock/release/owns_lock/mutex()` 成员
- **升级不存在**：持有共享锁时想写，必须 `release()`（或出作用域）再重新拿
  独占锁——中间有被写者插队的间隙，没有原子升级

### 2.3 规则细则

- `const` 读接口里也要加锁 → 互斥量声明为 `mutable` 成员
- 同一线程**递归** `lock_shared` 是 UB（不支持递归持锁）
- 持共享锁期间修改共享状态 = 数据竞争，编译器不会替你发现

## 三、实际应用与最佳实践

### 3.1 场景：读多写少的缓存（示例 ConfigCache）

读者拿 `shared_lock` 并行读，写者拿 `unique_lock` 独占改——
读写锁的教科书用法。示例 ③-2 用计时实证读者真的并行了
（两个各睡 150ms 的读者总耗时远小于串行的 300ms）。

### 3.2 最佳实践清单

1. **先量后换**：写频繁或临界区极短时，读写锁可能比普通 mutex 更慢
   （读者计数本身是原子竞争点）——有怀疑就 benchmark
2. 读侧 RAII 用 `shared_lock`；`lock_guard` 是独占，读路径禁用
3. 持共享锁期间只读不写；要写就老老实实走独占
4. 需要"升级"时释放重拿并接受间隙；被插队的数据用二次校验兜底
5. 多把锁场景注意加锁顺序一致性；timed 接口适合做诊断与重试，非常规武器

## 常见陷阱

1. **读者持锁写数据**——数据竞争，且编译器静默通过
2. `lock_guard<shared_timed_mutex>` 以为是读锁，实际是**独占写锁**
3. 同线程递归 `lock_shared` → UB
4. 依赖"写者优先"或"读者优先"——标准不保证，换平台行为就变
5. 想当然地"升级" shared→unique——接口不存在，手写升级是竞态温床

## 完整示例

见 `06_shared_timed_mutex.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织
（①用 try 系列验证四种互斥关系，③用两个线程的睡眠计时实证读者并行）：

```bash
g++ -std=c++14 -Wall -Wextra "CPP14/06_shared_timed_mutex与读写锁/06_shared_timed_mutex.cpp" -o run && ./run
```

## 练习

1. 计时对比：同一份只读数据，4 线程分别用 `std::mutex` 与
   `shared_timed_mutex` 各读 10 万次，比较总耗时。
2. 实现"降级"：持写锁改完数据后安全降为读者
   （提示：释放重拿的间隙里可能被插队，用二次校验兜底）。
3. 写一个稳健读函数：`try_lock_shared_for` 重试 3 次，全失败则改走独占锁。
4. 用 `std::mutex` + `condition_variable` 手搓一个读写锁（`rdlock`/`wrlock`），
   对照体会 C++14 标准化省掉了多少坑。

## 参考

- cppreference: [std::shared_timed_mutex](https://zh.cppreference.com/w/cpp/thread/shared_timed_mutex)
- cppreference: [std::shared_lock](https://zh.cppreference.com/w/cpp/thread/shared_lock)
- 《C++ Concurrency in Action》(Anthony Williams) 读写锁章节
