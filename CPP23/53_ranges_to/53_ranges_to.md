# 第 53 课 · ranges::to 与容器构造（C++23）

> 所需标准：**C++23**（`-std=c++23`；本机同前课需 `-lstdc++exp` 链接）｜
> 讲解：本文件 ｜ 独立代码：`53_ranges_to.cpp`
> 前置：第 33/34 课（视图与管道）；第 32 课（ranges 算法）

## 一、底层原理

### 1.1 视图的最后一公里难题

第 33 课的管道是惰性的——**要落成容器**（排序、随机访问、传给
老接口）就得自己写"先构容器再塞"：

```cpp
std::vector<int> out;
for (int x : v | views::filter(pred)) out.push_back(x);   // 样板
// 或 out.reserve + transform(back_inserter) 两步
```

视图是"过程"，很多时候终点要的是"结果"。C++23（P1206）
给管道装上终点站：**`ranges::to<C>()` 把任意区间物化成容器**——
元素类型自动推导（或显式指定），关联容器、string 全覆盖。

### 1.2 机制：三选一的构造策略

`to<C>(r)` 按目标容器能力自动选择路径：

1. 容器有**from_range 构造**（C++23 新增，`C(from_range_t, r)`）→ 直接用
2. 否则能 `emplace_back`（vector/string/deque）→ 逐个追加
3. 否则能 `insert`（map/set/list）→ 逐个插入

因此 `to` 不要求容器改造成"range aware"——新容器走新构造、
旧容器走兼容路径，一律可用。

### 1.3 调用形态：管道版与直调版

```cpp
auto a = r | std::ranges::to<std::vector>();       // 管道终点
auto b = std::ranges::to<std::vector>(r);          // 直接调用
auto c = r | std::ranges::to<std::vector<long>>(); // 显式元素类型
```

管道版要求**末尾 `()`**（`to<std::vector>` 是闭包工厂）——
这是与视图适配器 `take(3)` 同款的形态。

## 二、具体语法

```cpp
using std::ranges::to, std::views::filter, std::views::transform;

auto v = iota(1, 10) | filter(even) | to<std::vector>();     // vector<int>
auto s = iota(1, 7) | transform(mod3) | to<std::set>();      // 去重有序
auto m = vector<pair<string,int>>{...} | to<std::map>();     // pair 范围成 map
auto t = iota('a', 'f') | to<std::string>();                 // abcde（半开！）
auto tok = csv | views::split(',')
              | transform(stringify) | to<std::vector>();    // 第 34 课的现代版
auto d = r | to<std::deque<int>>();                          // 其他容器同理
```

规则细则：

- 元素类型**默认从 range 推导**；不合适（int→long 提升、
  string→string_view）就显式 `to<std::vector<long>>()`
- `to` **不带 reserve 参数**——超大批量要抠分配可手写
  reserve + 逐个塞（to 的方便与手写的性能二选一）
- 目标容器的比较器/哈希等额外模板参要写全：
  `to<std::set<int, std::greater<>>()`
- `to` 物化是**立即求值**：管道里的惰性到这一步全部兑现
- from_range 构造是 C++23 容器新标配（`C(std::from_range, r)`），
  自定义容器实现它即可被 `to` 高效使用

## 三、实际应用与最佳实践

### 3.1 场景：第 34 课 split 物化的现代版（示例 ②）

`split → transform(stringify) → to<vector>` 一条管道完成分词落地
——当年"手写循环 + push_back"的六行如今一行。

### 3.2 场景：词频统计全管道（示例 ③）

切词 → 落地 vector → 计数进 map → transform 去 const → 落地 → 排序。
**两条真实大坑**都在这里（见陷阱 4/5）：map 的 pair 带 const 键
不能直接排序；普通 lambda 不能接 `|`。

### 3.3 最佳实践清单

1. 管道终点要容器 → `to` 一发；要多趟消费先落地（第 33 课心法升级）
2. 元素类型"提升/转视图"需求 → 显式模板参
3. 一次性遍历的管道**保持视图**别落地——`to` 是终点站不是换乘站
4. map 排序前先 `transform` 去 const（或直接用 vector<pair> 起步）
5. 自定义容器实现 `from_range` 构造，融入 to 生态

## 常见陷阱

1. **管道版忘写末尾 `()`**：`| to<std::vector>` 编译错——
   `to<C>` 是工厂、`to<C>()` 才是可管道的闭包
2. **普通 lambda 接在 `|` 后面**：编译错——可管道的只有
   **范围适配器闭包**（C++23 的 range_adaptor_closure）；
   落地成容器后用普通调用写后续步骤
3. `to<std::string>(iota('a', 'f'))` 以为有 'f'——**iota 是半开区间**，
   得 "abcde" 五个字符
4. **map | to<vector> 后直接 sort：编译错**——元素是
   `pair<const K, V>`，const 键不可移动赋值，不满足 sortable；
   先 transform 成普通 pair（示例 ③ 的写法）
5. 期待 `to` 预留容量：没有 reserve 参数，多次增长照旧发生——
   极端性能场景手写 reserve 循环
6. `to` 之后原视图/容器失效规则不变：to 拷贝元素，
   落地物与源容器生命周期解耦（这点倒是安全）

## 完整示例

见 `53_ranges_to.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++23 -Wall -Wextra "CPP23/53_ranges_to/53_ranges_to.cpp" -o run -lstdc++exp && ./run
```

## 练习

1. 用 `to<std::map>` 把 `vector<pair<string,int>>` 物化，再写反向
   `map | to<vector>`（注意陷阱 4），对比两个方向的产物类型。
2. 把第 34 课练习 2 的"CSV 过滤空 token"改成
   `split | transform | to<vector>` 一行版。
3. 实验容量行为：一百万元素 `to<std::vector>` vs
   `reserve + 循环 push_back`，用容器 capacity() 对比增长次数。
4. 给自定义 `SmallVec<T, N>` 加 from_range 构造
   （`template<class R> SmallVec(std::from_range_t, R&&)`），
   验证 `r | to<SmallVec>()` 可用。

## 参考

- 提案 P1206R7（ranges::to 与 from_range）
- cppreference: [std::ranges::to](https://zh.cppreference.com/w/cpp/ranges/to)
- cppreference: [std::from_range_t](https://zh.cppreference.com/w/cpp/container/from_range_t)
