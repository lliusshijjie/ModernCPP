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

组合时**迭代能力取决于最弱一环**：管道里有 `filter` 整条退化为向前；
`reverse` 要求底层双向；`sort` 这类需要随机访问的算法接不了视图链——
需要排序就先落地成 `vector` 再排。

### 1.2 iota：把"计数"本身变成数据

`views::iota(1, 5)` 产出 1 2 3 4；`views::iota(1)` 是**无穷区间**
（哨兵类型是"可达无限"的空类型，正是第 32 课"哨兵可不同型"的用武之地）。
无穷源必须配 `take`/`take_while` 等截断才可消费——惰性让"无穷"安全。

### 1.3 split 与 join：切开与拍平的镜像

- `split(',')` 切出的是**子区间**（字符的区间），不是 `string`——
  零拷贝哲学；要 `std::string` 自己物化（`std::string(sub.begin(), sub.end())`）
- `join` 拍平**一层**：`vector<vector<int>>` → int 流。
  早期草案要求内层必须是左值；P2325R3 缺陷修正放开（本机 g++ 15 实测：
  `transform` 产出临时 vector 再 `join` 也能编译运行）

### 1.4 common / counted：新旧接口的转接头

C++17 及以前的算法要求 `begin()` 与 `end()` 同类型（common range）。
视图链经常不满足（如 `iota(1)` 的哨兵是另一类型）。`views::common`
把哨兵归一，`views::counted(iter, n)` 把"迭代器 + 个数"的老式接口
转成区间——两者都是给存量代码搭桥，新代码尽量直接用 ranges 算法。

## 二、具体语法（速查表）

| 视图 | 作用 | 要求/备注 |
|---|---|---|
| `views::all(r)` | 原样包装 | 左值→引用视图；右值→拥有视图 |
| `views::iota(a)` / `iota(a, b)` | 数列 | 单参无穷，须配 take |
| `views::filter(p)` | 谓词筛选 | 非 const 迭代（第 33 课） |
| `views::transform(f)` | 映射 | f 可为成员指针 |
| `views::take(n)` / `drop(n)` | 取/去前 n | O(1) 构造 |
| `views::take_while(p)` / `drop_while(p)` | 条件截头 | 首个不满足即停/续 |
| `views::reverse` | 反向 | 底层需双向 |
| `views::elements<I>`（`keys`/`values`） | 取 pair/tuple 成员 | map 两列视图 |
| `views::split(d)` | 按分隔切分 | 产出子区间 |
| `views::join` | 拍平一层 | 内层可为右值（P2325R3 后） |
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
2. 无穷源（单参 `iota`）**必须**有截断在下游，否则 range-for 死循环
3. `split` 后要 `string` 就显式物化；默认拿着子区间最省
4. 视图链的能力取短板：要随机访问/排序，先落地
5. `common`/`counted` 定位是适配器——新代码别主动制造对它们的依赖

## 常见陷阱

1. **`split` 的产物不是 string**：直接 `cout << sub` 编译错（它是字符区间）；
   `std::string(sub.begin(), sub.end())` 物化
2. 单参 `iota` 没接 `take` 就 range-for：死循环（无哨兵可达）
3. `reverse` 接在 `filter` 链后面没问题（filter 保双向），但接在输入流式
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
   3 的倍数的倒序，再解释为什么 `reverse` 不能放在 `iota(1)` 直接后面。
2. 把 `"a,b,,c"` 按逗号切分后**过滤空 token**，统计非空 token 个数。
3. 用 `keys` 视图收集 map 的键到 `vector`（range-for 或落地），
   与第 32 课投影方案对比。
4. 写 `flatten = views::join`，对 `vector<vector<int>>` 求全部元素和：
   先 `join` 再 `accumulate`（配 `common`），对比双层循环。

## 参考

- 提案 P0896R4（ranges）/ P2325R3（join_view 内层右值缺陷修正）
- cppreference: [范围适配器（视图）一览](https://zh.cppreference.com/w/cpp/ranges#范围适配器)
- cppreference: [views::iota](https://zh.cppreference.com/mwiki/index.php?title=cpp/ranges/iota_view&binding=zh)
