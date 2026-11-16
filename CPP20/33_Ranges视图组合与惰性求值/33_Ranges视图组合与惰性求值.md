# 第 33 课 · Ranges 视图组合与惰性求值（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`33_views_lazy.cpp`
> 前置：第 32 课（ranges 算法与投影）；对照第 20 课（string_view——同为"不拥有的视图"）

## 一、底层原理

### 1.1 视图是什么：不拥有、可廉价复制的区间

`view` 概念 = 可移动 + 移动/拷贝/begin/end 全是 O(1)。视图**不拥有元素**：
`filter/transform` 只是把"底层数据 + 筛选/映射规则"打包成一个瘦对象，
拷贝它就是拷贝几个指针。这与第 20 课 string_view 的设计哲学一脉相承——
借数据，不拿所有权。

### 1.2 惰性求值：构造管道时什么都不发生

```cpp
auto sq = v | std::views::transform(f);   // 此刻 f 一次都没被调用
```

管道只是"食谱"。真正迭代时才按需逐元素调用 f——而且**没有缓存**：
第二次迭代会把 f 再执行一遍（示例 ① 用调用计数器实证）。
`take(3)` 接在昂贵 transform 之后，上游只被拉动 3 次——
惰性天然带来**短路**（short-circuit），这是它对 eager 算法（第 32 课）的本质优势。

### 1.3 管道 | ：按数据流方向书写

```cpp
v | views::filter(p) | views::transform(f) | views::take(3)
// 等价于嵌套调用 take(transform(filter(v, p), f), 3) —— 但可读性天壤之别
```

每个适配器调用返回**范围适配器闭包（closure）**——一个捕获了参数、
等待区间来临的可调用对象；`|` 把区间喂给它。闭包本身还能再接 `|` 复用（示例 ②）。

### 1.4 所有权细则：谁养活视图引用的数据

- 视图引用的底层必须**活得比视图久**
- **右值容器**直接接管道：适配器把它移入 `owning_view` 自己养着——安全
  （但每次拷贝视图都是深拷贝语义，O(1) 性质没了）
- **局部变量**接管道后把视图**返回/存到外部**：视图还指着已亡栈帧——
  能编译，运行时 UB（本课头号陷阱，示例 ③ 注释演示）

## 二、具体语法

```cpp
using std::views::filter, std::views::transform,
          std::views::take, std::views::drop;

v | filter(pred);                    // 保留满足谓词的元素
v | transform(f);                    // 逐元素映射（成员指针也算 f）
v | take(n);  v | drop(n);           // 取前 n / 去前 n
v | take_while(pred);                // 头部连续满足段
v | drop_while(pred);                // 去掉头部连续满足段
auto head2 = take(2);                // 闭包可命名、可复用
auto mid    = drop(1) | take(2);     // 闭包可组合，v | mid 复用整条链
views::all(v);                       // 显式"包一层"（多用于库接口）
```

规则细则：

- 谓词/映射接收的是**元素的引用或值**：`filter` 谓词形参写成 `std::string&`
  会编译错——底层给的是 `const&`，谓词签名要用 `const std::string&` 或按值
- `transform` 的 f 可以是成员指针：`views::transform(&Person::name)`
  （与第 32 课投影同源：都走 `std::invoke`）
- **`filter_view` 迭代要求非 const**：C++20 中 `begin()` 非 const
  （它要缓存首个满足元素的位置）——`const auto v = ... | filter(...)` 后
  range-for 直接编译错；视图对象存成非 const
- 修改底层容器后再用旧视图 = 与旧迭代器同罪的 UB；视图不追踪底层的失效
- 惰性没有免费午餐：**反复消费同一管道 = 反复重算**；要多趟消费先落地成容器

## 三、实际应用与最佳实践

### 3.1 场景：一趟只看前几个的重活（示例 ①/③）

"百万日志取前 10 条 WARN"——`logs | filter(is_warn) | take(10)`：
找到 10 条后立即停，后面一个元素都不碰。eager 算法得先筛完一百万条。

### 3.2 场景：可复用的切片词汇（示例 ②）

`auto skip_header = drop(1); auto body = skip_header | take(100);`
把"去掉表头取正文"做成有名字的闭包，管道成了可组合的业务词汇。

### 3.3 最佳实践清单

1. 一次性查询用视图管道；**多趟消费**（排序后再查、反复遍历）先
   `拷贝落地`或用算法
2. 视图变量名暗示"它是借来的"：底层容器改名/销毁前先想视图还在不在
3. 局部容器上的视图不要逃出作用域；需要返回就返回容器或按值计算的结果
4. 谓词/映射签名统一写 `const T&`（或值）——既过编译又避免误改
5. 管道从左到右 = 数据从上游到下游：读管道就是读加工流程，写复杂嵌套调用是反模式

## 常见陷阱

1. **视图悬垂**：函数内 `return v | views::take(2);`（v 是局部变量）——
   编译通过、返回即 UB。右值接管道安全（owning_view 持有），
   但命名局部变量走的是 `ref_view` 引用路径，不拥有
2. **重复消费重算**：管道迭代两次 = 映射/谓词跑两轮；把管道当缓存用是性能事故
3. `const filter_view`（以及"对 const 视图对象 range-for"）编译错——
   不是 bug，是 C++20 接口约束；把视图对象声明为非 const
4. **谓词形参写非 const 引用**：`filter([](std::string& s){...})` 编译错，
   元素以 `const&` 到达
5. `transform` 里做**有副作用**的操作（计数、打印、累积状态）要想到：
   重迭代会重复副作用，短路会让部分元素"根本没被访问"——
   副作用与惰性组合出的执行次数最难推理

## 完整示例

见 `33_views_lazy.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra "CPP20/33_Ranges视图组合与惰性求值/33_views_lazy.cpp" -o run && ./run
```

## 练习

1. 复现示例 ① 的计数器实验：把 `take(3)` 挪到 `transform` 之前，
   观察调用次数变化，解释短路与管道顺序的关系。
2. 写一个可复用闭包 `even_squares = filter(even) | transform(square)`，
   分别作用于两个 vector，验证闭包不持有数据。
3. 故意写 `filter([](std::string& s){ return s.empty(); })` 应用于
   `vector<string>`，抄下编译错误；改成 `const&` 后通过。
4. 用 `views::iota(1) | views::take(8) | views::filter(...)` 生成前 8 个
   自然数中的偶数平方——全程不落容器（iota 详解见第 34 课）。

## 参考

- 提案 P0896R4（ranges；views 与适配器闭包）/ P2017R1（条件 borrowed）
- cppreference: [视图（views）](https://zh.cppreference.com/w/cpp/ranges#视图)
- cppreference: [range 适配器对象](https://zh.cppreference.com/w/cpp/ranges#range_适配器对象)
