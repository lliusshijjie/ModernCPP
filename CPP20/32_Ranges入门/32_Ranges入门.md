# 第 32 课 · Ranges 入门：新的算法与管道（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`32_ranges_algorithms.cpp`
> 前置：第 31 课（concepts——ranges 的整个接口都以概念约束）；第 25 课（旧算法与执行策略）

## 一、底层原理

### 1.1 旧算法的三个顽疾

C++98 起 `<algorithm>` 只认**迭代器对**：`std::sort(v.begin(), v.end())`。

| 顽疾 | 症状 | ranges 的解法 |
|---|---|---|
| 啰嗦 | 每次手写 begin/end | **直接传区间**：`std::ranges::sort(v)` |
| 按字段操作要写比较器 | `sort(v, [](a,b){ return a.age < b.age; })` 满屏 | **投影 projection**：`ranges::sort(v, {}, &Person::age)` |
| 迭代器悬垂无处防 | 对临时容器 `find` 返回指向亡灵的迭代器（UB） | **borrowed_range 检查**：返回 `ranges::dangling`，解引用直接编译错 |

### 1.2 迭代器 + 哨兵：end 不必再是"同一种迭代器"

ranges 算法的区间是 `[first, sentinel)`——**哨兵可以与迭代器类型不同**，
只要求"迭代器 == 哨兵"这个表达式合法。这解开了"必须先知道终点"的枷锁：
null 结尾的字符串、按条件终止、甚至**无穷序列**（第 33 课的 `views::iota`）
都能作为合法区间参与算法。

### 1.3 函数对象接口 + 全概念约束

`std::ranges::sort` 不是普通函数，而是**函数对象（niebloid）**：不经 ADL 查找，
用户代码无法劫持；所有重载都以第 31 课的概念（`range` / `random_access_range` /
`sized_range` / `borrowed_range`……）约束——传错了是一行人话报错，不是千行展开。
配套的 `ranges::begin/end/size/ssize/data` 是安全访问器：对内置数组、容器、
以及带自由 `begin()` 的类型统一处理。

### 1.4 投影（projection）：本次入门最重要的新武器

每个 ranges 算法最后都有个可选投影参数：**先把元素"投影"成比较目标，再参与操作**。
投影是任意可调用——成员指针 `&Person::age`、`[](auto& p){ return p.name; }` 都行。

```cpp
std::ranges::sort(v, {}, &Person::age);
//           元素↑  ↑比较器（{} = ranges::less 默认值）  ↑投影
```

**关键语义**：投影只改变"拿什么去比"，**被交换/移动的永远是元素本身**
——按 age 排序，`swap` 的是整个 Person，不是那个 int。

## 二、具体语法

```cpp
std::ranges::sort(v);                          // 区间版：容器直接进
std::ranges::sort(v.begin(), v.end());         // 迭代器+哨兵版仍然存在
std::ranges::sort(v, std::greater{}, proj);    // 比较器 + 投影自由组合

auto it = std::ranges::find(v, 8);             // 左值容器：真迭代器
auto d  = std::ranges::find(std::vector{1}, 1);// 右值容器：ranges::dangling
auto p  = std::ranges::find(ps, "ann", &Person::name);   // 投影查找
auto n  = std::ranges::count_if(ps, pred, &Person::age); // 投影计数
std::ranges::size(v);   // 无符号，要求 O(1)
std::ranges::ssize(v);  // 有符号（ptrdiff_t），for 循环下标友好
```

规则细则：

- **几乎所有** ranges 算法都接受投影（作为最后一个参数），对每个元素先投影再操作
- 投影结果只用于比较/判断；元素本身的交换、拷贝不受投影影响
- `ranges::size/ssize` 要求**常数时间**——`forward_list` 没有 size，直接编译错
- 对右值非 view 区间调用返回迭代器的算法 → 得到 `std::ranges::dangling`；
  它没有 `operator*`，悬垂从 UB 降级为编译错。`string_view` 等视图是
  `borrowed_range`，右值也安全（第 33 课展开）
- ranges 算法绝大多数是 **constexpr** 的（第 44 课配合编译期容器使用）
- 数值折叠没有 ranges 版：`accumulate` 仍是经典接口，`ranges::fold_left` 要等 C++23

## 三、实际应用与最佳实践

### 3.1 场景：结构体集合的全投影化操作（示例 ③）

按 `age` 排序、按 `name` 查找、按 `age` 计数、提取 `name` 列表——
四件事没有一个手写比较 lambda（示例 ③）。

### 3.2 场景：悬垂防御（示例 ①）

`ranges::find(std::vector<int>{1,2,3}, 2)` 不再悄悄返回野指针，
而是返回 `ranges::dangling`——把"运行时 UB"变成"编译期拒绝"。

### 3.3 最佳实践清单

1. 新代码一律 `std::ranges::xxx(容器)` 起步；只在需要子区间时写迭代器对
2. 按字段操作**先想投影**：`{}` 比较器 + `&Struct::field` 能顶掉八成比较 lambda
3. 对临时区间取结果前自问：它是不是 `borrowed_range`？不是就先存变量
4. 需要下标循环时用 `ranges::ssize`（有符号，与 `vector::difference_type` 一致）
5. 算法是**立即求值**的（eager）；要"流水线按需计算"用下一课的视图

## 常见陷阱

1. **投影排序不交换投影值**：`sort(ps, {}, &P::age)` 交换的是整个 Person——
   以为只动 age 是常见误解
2. 右值容器上调 `find/min_element` 等：能编译（返回 dangling 是合法路径），
   但解引用 dangling 编译错——这是**故意的**，把旧时代的 UB 显式化
3. `ranges::count(v, "abc")`：值与元素要能 `==`——`const char*` 与元素
   `const char*` 比的是**指针**，与 `std::string` 比的才是内容
4. `ranges::size` 在链式结构上编译错不是 bug：O(1) 语义是接口契约的一部分
5. `ranges::sort` 只支持 `random_access_range`：传 `list`/`forward_list` 编译错
   ——链表排序本来就该用成员 `sort()`（稳定性与节点交换语义不同）

## 完整示例

见 `32_ranges_algorithms.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra "CPP20/32_Ranges入门/32_ranges_algorithms.cpp" -o run && ./run
```

## 练习

1. 给 `vector<pair<string,int>>` 按 `second` 降序排序，再用投影按 `first`
   查找一个名字——全程不写比较 lambda。
2. 把 `ranges::find(std::vector<int>{7,8,9}, 8)` 的返回类型抄下来（`typeid` 或
   IDE 悬停），再故意解引用它，记录编译器的报错原文。
3. 用 `count_if` + 投影统计一个 `vector<Person>` 里年龄 ≥ 30 的人数，与手写
   循环对比。
4. 把第 25 课使用过的三个旧算法调用改写成 ranges 版，对比行数与可读性。

## 参考

- 提案 P0896R4（The One Ranges Proposal，ranges 与约束算法入 C++20）
- cppreference: [约束算法 std::ranges](https://zh.cppreference.com/w/cpp/algorithm/ranges)
- cppreference: [投影（projection）](https://zh.cppreference.com/w/cpp/algorithm/ranges#投影)
- cppreference: [ranges::dangling](https://zh.cppreference.com/w/cpp/ranges/dangling)
