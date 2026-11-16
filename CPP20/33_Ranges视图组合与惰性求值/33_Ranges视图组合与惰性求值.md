# 第 33 课 · Ranges 视图组合与惰性求值（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`33_views_lazy.cpp`
> 前置：第 32 课（ranges 算法与投影）；对照第 20 课（string_view——非拥有视图的一个例子）

## 一、底层原理

### 1.1 视图是什么：适合组合适配器的轻量区间

`std::ranges::view<T>` 要求 `range<T> && movable<T> && enable_view<T>`，
并有复杂度契约：移动构造 O(1)，若可拷贝则拷贝构造也须 O(1)；不要求
可拷贝或默认构造。视图可以借用、拥有或生成元素：`ref_view` 借用容器，
`owning_view` 拥有容器，`iota_view` 按需生成值。`filter/transform` 保存底层
视图和规则，所有权随底层视图而定。

本课按应用了 C++20 缺陷修正的标准库讲解：P2325R3 去掉默认构造要求，
P2415R2 引入 `owning_view` 并放宽析构/赋值的复杂度规则；旧库支持可能不同。
`begin/end` 的复杂度由各接口约定，不能把首次筛选等操作都视为无条件 O(1)。

### 1.2 惰性求值：通常在遍历时计算元素

```cpp
auto sq = v | std::views::transform(f);   // 此刻 f 一次都没被调用
```

此处构造 `transform_view` 不调用 f；适配器构造仍可能拷贝/移动规则对象，
不能说"什么都不发生"。解引用变换迭代器时才调用 f，且不缓存变换值：
重复解引用或再次遍历都会重算（示例 ① 用计数器观察）。
视图也可能缓存位置，例如 forward range 上的 `filter_view` 缓存首个匹配位置。
本例完整遍历 `sq | take(3)` 只做 3 次变换，但若有 `filter`，其迭代器递增
可能额外扫描后续元素；不要把 take 数量当成通用的谓词/映射调用次数保证。

### 1.3 管道 | ：按数据流方向书写

```cpp
v | views::filter(p) | views::transform(f) | views::take(3)
// 等价于嵌套调用 take(transform(filter(v, p), f), 3) —— 但可读性天壤之别
```

`views::filter(p)`、`views::take(n)` 这类部分应用返回**范围适配器闭包（closure）**——一个捕获了参数、
等待区间来临的可调用对象；`|` 把区间喂给它。闭包本身还能再接 `|` 复用（示例 ②）。

### 1.4 所有权细则：谁养活视图引用的数据

- 对借用数据的视图，使用期间底层数据及所需迭代器必须有效
- P2415R2 修正后，**可移动的右值非 view 容器**（如 `vector`）可移入
  `owning_view`；它只可移动、不可拷贝，不会发生视图深拷贝。
  右值 `initializer_list` 不走这条路径；右值 `string_view` 仍然只借用字符
- **局部变量**接管道后把视图**返回/存到外部**：视图还指着已亡栈帧——
  可以编译，但结果悬垂，后续访问可能触发 UB（本课头号陷阱）

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

- 谓词/映射接收底层迭代器解引用的结果：非 const `vector<string>` 提供
  `string&`，`filter([](string& s){ return s.empty(); })` 可以编译，
  const 容器才提供 `const string&`。只读查询推荐 `const T&`，谓词不得
  修改被检查对象，且须满足相等保持等语义要求
- `transform` 的 f 可以是成员指针：`views::transform(&Person::name)`
  （与第 32 课投影同源：都走 `std::invoke`）
- **`filter_view` 迭代要求非 const**：C++20 中 `begin()` 非 const
  （底层为 forward range 时会缓存首个满足元素的位置）——`const auto v = ... | filter(...)` 后
  range-for 直接编译错；视图对象存成非 const
- 修改底层不一定使视图失效，需遵守底层迭代器的失效规则。视图不会追踪
  失效，也不会自动刷新 filter 的缓存；改变筛选结果后可能需要重建视图
- 惰性没有免费午餐：**反复消费同一管道 = 反复重算**；要多趟消费先落地成容器

## 三、实际应用与最佳实践

### 3.1 场景：一趟只看前几个的重活（示例 ①/③）

"百万日志取前 10 条 WARN"——`logs | filter(is_warn) | take(10)`：
限制最多产出 10 条，避免物化全部筛选结果；不过 filter 迭代器在最后一次
递增时仍可能扫描后续日志。对昂贵谓词或严格访问预算，需单独分析遍历行为。

### 3.2 场景：可复用的切片词汇（示例 ②）

`auto skip_header = drop(1); auto body = skip_header | take(100);`
把"去掉表头取正文"做成有名字的闭包，管道成了可组合的业务词汇。

### 3.3 最佳实践清单

1. 一次性查询用视图管道；**多趟消费**（排序后再查、反复遍历）先
   `拷贝落地`或用算法
2. 明确视图是借用还是拥有数据；借用时关注底层对象的销毁与迭代器失效
3. 局部容器上的视图不要逃出作用域；需要返回就返回容器或按值计算的结果
4. 谓词/映射签名统一写 `const T&`（或值）——既过编译又避免误改
5. 管道从左到右 = 数据从上游到下游：读管道就是读加工流程，写复杂嵌套调用是反模式

## 常见陷阱

1. **视图悬垂**：函数内 `return v | views::take(2);`（v 是局部变量）——
   编译通过但结果悬垂，使用时才会触发 UB。可移动右值容器可由 owning_view 持有，
   但命名局部变量走的是 `ref_view` 引用路径，不拥有
2. **变换值不缓存**：重复消费会重算映射；谓词可能因位置缓存少跑一些，
   不要依赖精确调用次数或把整个管道当值缓存
3. 对 `const filter_view` 做 range-for 会编译错；其他 view 可能支持 const 迭代——
   不是 bug，是 C++20 接口约束；把视图对象声明为非 const
4. **把非 const 视图与可写元素混同**：filter_view 的对象需要非 const
   才能迭代，但元素是否可写仍由底层区间决定
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
   验证本例调用次数不变；再加入 filter，观察筛选与变换的顺序如何影响计算量。
2. 写一个可复用闭包 `even_squares = filter(even) | transform(square)`，
   分别作用于两个 vector，验证闭包不持有数据。
3. 把 `filter([](std::string& s){ return s.empty(); })` 分别应用于
   非 const 与 const 的 `vector<string>`，验证前者通过、后者失败；再改成 `const&`。
4. 用 `views::iota(1) | views::take(8) | views::filter(...) | views::transform(...)`
   生成前 8 个自然数中的偶数平方——全程不落容器（iota 详解见第 34 课）。

## 参考

- 提案 P0896R4（ranges；views 与适配器闭包）/ P2017R1（条件 borrowed）
- [P2415R2：What is a view?（owning_view 与复杂度修正）](https://wg21.link/p2415r2)
- [P2325R3：视图不应要求默认构造](https://wg21.link/p2325r3)
- cppreference: [视图（views）](https://zh.cppreference.com/w/cpp/ranges#视图)
- cppreference: [range 适配器对象](https://zh.cppreference.com/w/cpp/ranges#range_适配器对象)
