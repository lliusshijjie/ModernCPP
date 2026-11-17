# 第 45 课 · std::atomic 的等待与通知（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`45_atomic_wait_notify.cpp`
> 前置：第 38 课（jthread/stop_token——本课线程骨架）；第 23 课（锁与同步）

## 一、底层原理

### 1.1 旧世界的两难：自旋 or 条件变量

"等一个原子变量变成某值"只有两条老路：

| 方案 | 代价 |
|---|---|
| 自旋 `while (!flag.load()) ;` | 烧满一个核、延迟抖动（加 sleep 又拖慢响应） |
| `condition_variable` + mutex | 必须配锁与谓词循环；丢唤醒/伪唤醒要人肉防；为一个 int 拖一整套机制 |

C++20 给 `std::atomic` 原生装上 `wait` / `notify_one` / `notify_all`
（Linux 走 futex、Windows 走 WaitOnAddress）：**内核态挂起、值驱动
唤醒、免锁免谓词循环**——单值等待从此两行。

### 1.2 wait(old) 的精确语义：先查值，再睡觉

```
wait(old) ≈ while (load() == old) 系统级阻塞(被通知后醒来重查)
```

三个直接推论：

1. **丢通知免疫**：先改值后通知，就算通知赶在 wait 之前发出，
   wait 进门看到值已变，立即返回（示例 ①）——条件变量的
   "通知先于等待"丢唤醒问题在此结构性不存在
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
v.wait(0);              // 阻塞到 v.load() != 0（且被通知）
v.notify_one();         // 唤醒一个等待者
v.notify_all();         // 唤醒全部

std::atomic_flag gate{};
gate.test_and_set();    // 置位，返回旧值
gate.clear();           // 清零
gate.wait(true);        // 等"不再是 true"
gate.notify_one();
// C++20：atomic_flag 默认构造即清零态；ATOMIC_FLAG_INIT 宏已废弃

// 自由函数版（操作地址，供跨语言/裸对象场景）：
std::atomic_wait(&v, 0);
std::atomic_notify_one(&v);
```

规则细则：

- `wait` 要求 `T` **平凡可拷贝**；比较按**值表示**（逐位）进行
- 内部 load 是顺序一致的（seq_cst）——与默认 `load()` 一致
- `atomic_ref<T>`（C++20）同样有 wait/notify，可与 atomic 协同
- 等待多变量/复杂谓词（"a 或 b"、"求和 > n"）**不适合**：
  回条件变量 + 锁的世界
- 一个对象上同时混用 wait 与锁体系不冲突（各等各的），
  但设计上选一边

## 三、实际应用与最佳实践

### 3.1 场景：数据就绪握手（示例 ③）

生产者 `data.store(x); ready.store(true); ready.notify_one();`
消费者 `ready.wait(false); 读 data`——两原子、零锁、零条件变量。
比"互斥量 + cv"少约一个数量级的样板（示例 ③ 全部逻辑 10 行）。

### 3.2 场景：轻量任务关卡（示例 ②）

worker 等待"开工位"翻转：`gate.wait(true)`——主线程 `clear() +
notify_one()` 放行。atomic_flag 是最小的同步原语（常为单字节指令级）。

### 3.3 最佳实践清单

1. **单值等待**（标志、计数、游标）首选 atomic wait；
   复杂状态谓词仍用 cv/锁——别硬凑
2. 顺序永远"先改值、后通知"；多处改值共用一个通知点
3. 唤醒后**重读相关变量**再决策（醒来时世界已变——
   对照第 38 课"检查点"思维）
4. 多消费者竞取用 `notify_all` + 值轮转（如环形游标）；
   `notify_one` 只在"单消费者"语义明确时用
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
   `wait_for/wait_until`（那是 C++30 都未必有的）——要超时
   自己加"超时标志 + notify"或改 cv
5. 唤醒后直接用旧读取的值：醒来意味着"值可能变了"，
   必须 `load()` 重读（示例 ③ 消费者的写法）
6. `atomic<double>` 等**按位比较**：`-0.0` 与 `+0.0` 位模式不同，
   `wait(-0.0)` 遇 `+0.0` 会醒——数值语义敏感处改整型标记

## 完整示例

见 `45_atomic_wait_notify.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra "CPP20/45_atomic等待与通知/45_atomic_wait_notify.cpp" -o run && ./run
```

## 练习

1. 复现"先通知后改值"事故：两个线程故意反序执行，观察等待者
   长睡不醒；再把两行换序验证恢复。
2. 用 `notify_all` + 计数游标写"三线程接力"：atomic<int> 从 0 走到 3，
   每个线程只处理自己的号段。
3. 对比实验：自旋版 `while(load()==0);` 与 `wait` 版的 CPU 占用
   （任务管理器观察），写一段结论。
4. 给第 38 课练习 4 的"等数据或停止"改造：用两个 atomic
   （数据 + 停止）双 wait 轮询替代 cv_any——对比两版复杂度
   （提示：两个 wait 无法同时等，需要小自旋间隔，体会边界）。

## 参考

- 提案 P1135R6（atomic 的等待与通知）
- cppreference: [std::atomic::wait](https://zh.cppreference.com/w/cpp/atomic/atomic/wait)
- cppreference: [std::atomic_notify_one](https://zh.cppreference.com/w/cpp/atomic/atomic_notify_one)
