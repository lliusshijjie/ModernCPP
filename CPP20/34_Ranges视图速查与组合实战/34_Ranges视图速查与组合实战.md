# 第 34 课 · Ranges 视图速查与组合实战（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`34_views_cookbook.cpp`
> 前置：第 33 课（视图惰性与管道机制）；第 32 课（投影）

## 一、底层原理

### 1.1 视图家族全景：三种角色

C++20 的 `<ranges>` 视图按角色分三类：

- **源**：`iota`（数列）、`single`/`empty`（单元素/空）——凭空生成数据
- **变换**：`filter` / `transform` / `reverse` / `elements`——逐个或整体改形
- **切分与拼接**：`take` 家族（截断）、`split`（切开）、`join`（拍平）、
  `common`/`counted`（与旧接口搭桥）

组合时需逐层检查迭代能力：`filter` 可保留双向能力，但会丢失随机访问；
`transform` 在相应底层上可保留随机访问，`reverse` 要求双向区间。
`ranges::sort` 可以直接排序满足随机访问且迭代器满足 `sortable` 的视图，
例如可写 vector 的 `take` 视图；这会修改原容器。能力不足或需要独立结果时
再物化为 `vector`。仅有随机访问还不够，返回临时值的变换通常不可排序。

### 1.2 iota：把"计数"本身变成数据

`views::iota(1, 5)` 产出 1 2 3 4；`views::iota(1)` 是**无穷区间**
（哨兵是 `std::unreachable_sentinel_t`，永不与迭代器相等）。
消费时需要 `take`、能终止的条件或显式 break。整数类型仍有界，
`iota(1)` 若不断递增至 int 上限会有有符号溢出 UB，惰性不会消除数值边界。

### 1.3 split 与 join：切开与拍平的镜像

- `split(',')` 切出的是**子区间**（字符的区间），不是 `string`——
  零拷贝哲学；要 `std::string` 自己物化（`std::string(sub.begin(), sub.end())`）
- `join` 拍平**一层**：`vector<vector<int>>` → int 流。
  原始 C++20 支持内层为 glvalue 区间或 prvalue 视图；**P2328R1**
  缺陷修正进一步支持 prvalue 非 view 区间，如 transform 返回临时 vector。
  此类 join 通常只有 input range 能力。
- 本课 `split` 按 **P2210R2** 缺陷修正后的接口讲解：要求 forward range，
  产出迭代器子区间；旧版库可能仍是修正前的设计

### 1.4 common / counted：新旧接口的转接头

C++17 及以前的算法要求 `begin()` 与 `end()` 同类型（common range）。
视图链经常不满足（如 `iota(1)` 的哨兵是另一类型）。`views::common`
把哨兵归一，`views::counted(iter, n)` 把"迭代器 + 个数"的老式接口
转成区间——两者都是给存量代码搭桥，新代码尽量直接用 ranges 算法。

## 二、具体语法（速查表）

| 视图 | 作用 | 要求/备注 |
|---|---|---|
| `views::all(r)` | 包装成视图 | 已是 view 则按值构造；非 view 左值通常引用，适合的右值经 owning_view 拥有（P2415R2） |
| `views::iota(a)` / `iota(a, b)` | 数列 | 单参哨兵不可达，需终止消费并防止数值溢出 |
| `views::filter(p)` | 谓词筛选 | 非 const 迭代（第 33 课） |
| `views::transform(f)` | 映射 | f 可为成员指针 |
| `views::take(n)` / `drop(n)` | 取/去前 n | n 须非负；首次定位可能要遍历 |
| `views::take_while(p)` / `drop_while(p)` | 条件截头 | 首个不满足即停/续 |
| `views::reverse` | 反向 | 底层需双向 |
| `views::elements<I>`（`keys`/`values`） | 取 pair/tuple 成员 | map 两列视图 |
| `views::split(d)` | 按分隔切分 | 产出子区间 |
| `views::join` | 拍平一层 | 内层可为右值（P2328R1 后） |
| `views::common` | 哨兵归一 | 接 C++17 算法/接口 |
| `views::counted(it, n)` | 迭代器+个数转区间 | 兼容指针式 API |
| `views::single(x)` / `views::empty<T>` | 单元素/空 | 测试与占位 |

## 三、实际应用与最佳实践

### 3.1 场景：文本分词管道（示例 ③）

`sentence | split(' ') | transform(stringify) | filter(len>3)`——
分词、物化、筛选一条管道写完，零中间容器；同一条链再接
`transform(&std::string::size)` 就得到长度序列（示例 ③）。

### 3.2 场景：表格的两列视图（示例 ①）

`map` 的 `keys`/`values` 让"键集合"与"值集合"直接可迭代——
统计值合计不必再写循环取 `second`。

### 3.3 最佳实践清单

1. 管道末端才决定形态：要容器就落地，要遍历就保持视图
2. 单参 `iota` 需要明确终止消费的方式，并确保数值递增不溢出
3. `split` 后要 `string` 就显式物化；默认拿着子区间最省
4. 先检查视图是否满足算法的迭代器与可写性要求；不满足时再落地
5. `common`/`counted` 定位是适配器——新代码别主动制造对它们的依赖

## 常见陷阱

1. **`split` 的产物不是 string**：直接 `cout << sub` 编译错（它是字符区间）；
   `std::string(sub.begin(), sub.end())` 物化
2. 单参 `iota` 不会因到达哨兵而停止；无限递增 int 最终会溢出，不能当作安全死循环
3. 底层是双向区间时，`reverse` 可接在 `filter` 后；但接在输入流式
   视图后面编译错——能力短板取决于"最弱"视图
4. `join` 只拍**一层**：三层嵌套要 `join | join` 两次
5. `views::counted(p, n)` 不检查 n 越界——它只是区间包装，越界照样 UB；
   边界要自己保证

## 完整示例

见 `34_views_cookbook.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra "CPP20/34_Ranges视图速查与组合实战/34_views_cookbook.cpp" -o run && ./run
```

## 练习

1. 用 `iota(1) | take(20) | filter(3 的倍数) | reverse` 产出前 20 个自然数中
   3 的倍数的倒序，再解释为什么 `reverse` 直接作用于 `iota(1)` 无法获得可达的末尾
   （可能无法终止并最终溢出，并非仅因类型约束而编译失败）。
2. 把 `"a,b,,c"` 按逗号切分后**过滤空 token**，统计非空 token 个数。
3. 用 `keys` 视图收集 map 的键到 `vector`（range-for 或落地），
   与第 32 课投影方案对比。
4. 写 `flatten = views::join`，对 `vector<vector<int>>` 求全部元素和：
   先 `join` 再 `accumulate`（配 `common`），对比双层循环。

## 参考

- 提案 P0896R4（ranges）
- [P2328R1：join_view 支持 prvalue 非 view 内层区间](https://wg21.link/p2328r1)
- [P2210R2：split_view 的缺陷修正](https://wg21.link/p2210r2)
- [P2415R2：owning_view 与 views::all](https://wg21.link/p2415r2)
- cppreference: [范围适配器（视图）一览](https://zh.cppreference.com/w/cpp/ranges#范围适配器)
- cppreference: [views::iota](https://zh.cppreference.com/mwiki/index.php?title=cpp/ranges/iota_view&binding=zh)
