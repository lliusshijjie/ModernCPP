# 第 57 课 · std::flat_map / flat_set（C++23）

> 所需标准：**C++23**（`-std=c++23`；本机 g++ 15.1.0 **已提供 `<flat_map>`**
> （GCC 15 实现），本课全部真实接口；链接 `-lstdc++exp` 系列惯例）｜
> 讲解：本文件 ｜ 独立代码：`57_flat_containers.cpp`
> 前置：第 30 课（map 节点句柄——flat 版没有节点，对照着学）；
> 第 43 课（erase_if 的容器适配面）

## 一、底层原理

### 1.1 树与数组的百年之争，由"用法"裁决

map/set 底层是**红黑树**：增删 O(log n) 稳定、指针跳转毁缓存、
每个节点三次分配级别的心智负担。而真实工程里的映射表大多是
**"批量构建 + 海量查找 + 极少增删"**——排序数组二分查找
（O(log n) + 缓存行友好 + 迭代零跳转）全面占优。

`std::flat_map / flat_set`（P0429）：**接口与 map/set 完全对齐**，
底层换成有序扁平容器（实现常用排序 vector；libstdc++ 的 flat_map
用**键、值两个分离 vector**）。一句话：**map 的脸，数组的身**。

| | map/set（树） | flat_map/flat_set（扁平） |
|---|---|---|
| 查找 | O(log n)，指针跳转 | O(log n)，缓存友好（更快） |
| 迭代 | 中序遍历，跳跃 | 顺序扫描，**随机访问迭代器** |
| 中段插入/删除 | O(log n) 指针手术 | **O(n) 搬移**（数组的代价） |
| 迭代器失效 | 仅删除点 | 插入/删除点之后**全体失效** |
| 节点句柄 | extract/merge（第 30 课） | **没有**（数组无节点可拔） |
| 内存 | 节点散布 + 指针开销 | 紧凑连续 |

### 1.2 键值分离的代理迭代器（libstdc++ 实现细节）

libstdc++ 的 flat_map 把键、值放**两个平行 vector**——元素不是
`pair<K,V>` 对象，迭代时产生**代理**：`pair<const K&, V&>` 的右值。
推论：`for (auto& [k, v] : fm)` 编译错（右值绑不上非 const 引用），
要写 `const auto&` 或 `auto&&`（示例 ① 的写法）。

### 1.3 迭代器能力：新概念 vs 旧标签（本机实测）

- C++20 概念：flat_set 与 flat_map 的迭代器都满足
  `std::random_access_iterator` ✓
- 旧式 `iterator_category` 标签：flat_set 是
  `random_access_iterator_tag`；flat_map 因代理引用**标签降级**
  （本机实测非 RA tag）
- 结论：泛型代码用**概念/ ranges**分发（第 31/32 课风格）无忧；
  依赖旧标签特化的远古代码要留意

## 二、具体语法

```cpp
std::flat_map<std::string, int> fm;      // 默认：vector 底层 + less
fm["k"] = 1;  fm.at("k");  fm.contains("k");
std::flat_set<int> fs{5, 1, 3, 1};       // 重复被忽略：{1,3,5}

std::flat_map<int, std::string> built(raw.begin(), raw.end()); // 批量构建
fm.lower_bound(k);  fm.equal_range(k);   // 有序区间的力量
std::flat_set<std::string, std::greater<>> desc{...};  // 自定义比较器

// 没有的东西（对照 map）：extract / merge / 节点句柄——底层是数组
// erase_if 有：std::erase_if(flat_map, pred)（第 43 课接口面）
```

规则细则：

- **迭代恒按键序**（比较器说了算），插入乱序不影响
- 批量构建优于逐个 insert：每 insert 中段都可能整体搬移
- `insert_or_assign / try_emplace / operator[]` 语义与 map 一致
  （对照第 30 课的"修补"清单）
- 修改元素的 key 不可能（同 map）；值可改（`it->second = x`）
- 容器适配器形态：底层可换成 `std::deque` 等
  （flat_map 的模板参指定容器类型——概念上需随机访问）
- 透明比较器（`std::less<>`）支持异构查找——与 map 同规

## 三、实际应用与最佳实践

### 3.1 场景：配置表/枚举映射/消息字典（示例 ③）

启动时一次构建、运行期纯查找——flat_map 查找比 map 快一截
（缓存友好 + 无指针追踪），内存还小。示例 ③ 的 HTTP 状态字典
就是标准画像。

### 3.2 场景：批量构建 + 有序扫描（示例 ②）

`flat_map(first, last)` 从排序区间直达；lower_bound/equal_range
配合起来就是"范围查询"——排行榜、分桶、时间窗都顺手。

### 3.3 最佳实践清单

1. 选型口诀：**读多写少走 flat，写热路径留树**
2. 大批量初始化用迭代器区间构造，别循环 insert
3. 迭代解构写 `const auto&`（代理引用，跨实现最稳）
4. 泛型代码用 C++20 概念（random_access_iterator）判断迭代器
   能力，别读旧式 iterator_category
5. 需要节点搬家的场景（第 30 课 extract/merge）flat 不可用——
   老实用 map

## 常见陷阱

1. **写热路径用 flat**：百万级中段插入 = 百万次 O(n) 搬移——
   性能直接塌方；写多请回 map
2. `for (auto& [k, v] : flat_map)`（非 const auto&）在键值分离
   实现上编译错——代理右值绑不上；养成 `const auto&` 习惯
3. **迭代器失效面**：中段插入/删除使之后的迭代器全部失效
   （map 只失效被删点）——持有迭代器跨修改的操作要重取
4. 找 extract/merge：不存在——数组实现没有节点概念，编译错提醒你
5. 预期 unordered_map 的"无序快查"：flat 是**有序**结构，
   与 unordered 是两套权衡（无序哈希查找 O(1) 但迭代无序、
   内存开销大——三方取舍按场景）
6. 旧式标签分发代码把 flat_map 迭代器当非 RA 处理（本机标签降级）
   ——用现代概念或直接依赖 ranges 算法

## 完整示例

见 `57_flat_containers.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++23 -Wall -Wextra "CPP23/57_flat_map与flat_set/57_flat_containers.cpp" -o run -lstdc++exp && ./run
```

## 练习

1. 十万个随机键分别插入 map 与 flat_map（一次构建），
   各做一百万次 contains——计时对比（提示：构建也分开计）。
2. 复现陷阱 2 与陷阱 3：各写一段最小代码，
   记录编译错信息与迭代器失效的表现。
3. 用 flat_map + equal_range 实现排行榜的"分数段查询"
   （返回 [lo, hi) 区间内的玩家名）。
4. 把 flat_map 的底层容器换成 deque（模板参指定），
   观察哪些操作仍可用、哪些能力变化。

## 参考

- 提案 P0429R9（flat_map / flat_set 容器适配器）
- cppreference: [std::flat_map](https://zh.cppreference.com/w/cpp/container/flat_map)
- cppreference: [std::flat_set](https://zh.cppreference.com/w/cpp/container/flat_set)
