# 第 43 课 · std::erase / std::erase_if 统一擦除（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`43_erase_if.cpp`
> 前置：第 25 课（旧算法面貌）；第 32 课（ranges——同为"算法吃容器"的新姿势）

## 一、底层原理

### 1.1 旧世界的两套"擦除芭蕾"

**序列容器**（vector/string）：按值删除要走 remove-erase 惯用法——
`v.erase(std::remove_if(v.begin(), v.end(), pred), v.end())`。
remove 只是把保留元素前移、返回新逻辑尾，erase 才真正截短——
两步合舞、忘写 erase 是经典 bug（size 不变、尾部是"僵尸值"）。

**关联容器**（map/set）：根本没有 remove，只能"迭代器循环 + erase"，
还得小心 `it = m.erase(it)` 的推进规则——写错一个自增就 UB。

C++20 的 `std::erase_if(c, pred) / std::erase(c, value)` 把两套舞蹈
统一成**一行标准件**：返回删除个数、序列容器保序、关联容器按树序高效删。

### 1.2 为什么是自由函数而不是成员？

map 的 `erase(key)` 只删**一个键**；"按谓词删一批"语义需要新名字。
标准选择放进 `std::` 命名空间做**自由函数重载**（Library Fundamentals 2
移植），好处：泛型代码里 `std::erase_if(c, p)` 对任意容器同一句法——
连 `std::string` 都有份（按字符谓词清串）。

### 1.3 性能面：与惯用法等价、与成员互补

vector 版内部就是移位压缩（和 remove-erase 同复杂度、常量更好——
一次遍历计数后批量搬）；map 版在树遍历中直接摘节点，
**没有**"迭代器失效舞步"。注意它与单点成员各司其职：
按迭代器/键删单个仍用 `c.erase(...)`。

## 二、具体语法

```cpp
auto n1 = std::erase_if(vec, pred);      // vector：保序，返回个数
auto n2 = std::erase(vec, value);        // vector/string：删所有匹配
auto n3 = std::erase_if(mymap,
                 [](const auto& kv) { ... });   // pair<const K, V>
auto n4 = std::erase_if(myset, pred);
auto n5 = std::erase_if(str, [](char c) { ... });  // string
auto n6 = std::erase(str, 'x');          // string：删某字符全部出现
// std::array 没有重载（定长）；deque/list/forward_list/
// unordered_* 均有
```

规则细则：

- `std::erase(v, value)` 对 vector 是**删除全部等值元素**——
  不是成员 `erase` 的"删第一个"语义，两者别混
- map 谓词形参是 `pair<const K, V>`：写 `[](const auto& kv)` 最稳，
  结构化绑定取 `kv.first / kv.second`
- 返回类型是容器 `size_type`（删除个数）——日志、统计直接用
- 序列容器**保持相对顺序**；关联容器删后仍是按序状态
- 谓词里**不要修改容器本身**（既改又删是 UB 级骚操作）
- 空容器 / 全删光：返回 0 / size()，无特殊情况

## 三、实际应用与最佳实践

### 3.1 场景：状态清理（示例 ③）

"清掉所有已完成订单 / 失效连接 / 空标签"——三行以内说完的清洗逻辑
以前要六行芭蕾；配合返回值还能顺手记日志
（`log("cleaned %d orders")`）。

### 3.2 场景：map 批量裁剪（示例 ②）

按 value 条件删 key-value（库存清零、过期项）——旧代码的
`for (it = m.begin(); it != m.end(); ) { if (...) it = m.erase(it); else ++it; }`
整段退役。

### 3.3 最佳实践清单

1. 任何"按谓词/按值批量删"一律 `std::erase_if / std::erase`；
   新代码禁止出现 remove-erase 惯用法
2. 删单个已知元素（迭代器/键）继续用成员 `erase`——单点操作它更直接
3. 删除同时要**处理**被删元素（回调、析构前动作）→ 先 for 处理再
   erase_if，或用 ranges 收集后统一处理
4. 泛型容器参数的函数模板里可直接 `std::erase_if`——
   各容器都有重载，句法统一
5. 删除条件依赖位置/索引（删第偶数个）erase_if 不合适——
   它只看元素值

## 常见陷阱

1. **以为 `std::erase(v, x)` 删第一个**——它删**全部**匹配；
   只删第一个要先 `std::find` 再成员 `erase`
2. map 谓词写成 `[](const std::pair<std::string, int>& kv)` 编译错：
   键是 `const K`，签名对不上——用 `const auto&` 或
   `pair<const std::string, int>`
3. 旧习难改：remove-erase 忘了 erase 那半截——现在整句换成
   `std::erase_if` 就不存在忘不忘的问题
4. 在谓词里**修改正在被遍历的容器**（push_back/erase）——UB；
   想边删边补就分两轮
5. `std::array` / 原生数组没有此接口：定长容器不存在"擦除后变短"，
   编译错是正确行为
6. 以为 erase_if 后"容量缩了"——vector 只缩 size 不缩 capacity；
   要真释放 `shrink_to_fit`（或换新对象）

## 完整示例

见 `43_erase_if.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra "CPP20/43_erase_if统一擦除/43_erase_if.cpp" -o run && ./run
```

## 练习

1. 分别用 remove-erase 惯用法与 `std::erase_if` 实现"删所有负数"，
   对比行数，再故意只写 remove 不写 erase 观察僵尸值现象。
2. 写 `prune(map<string,int>&, int cap)`：删除所有 value < cap 的项，
   返回删除数；调用后遍历打印验证。
3. 对 `vector<string>` 先 `std::erase(v, "")` 再
   `std::erase_if(v, [](s){ return s.size() > 10; })`——
   两步顺序换个先后，结果会不同吗？为什么（提示：两谓词独立）。
4. 实现一个泛型 `clean(C& c)`：`if constexpr`（第 13 课）区分
   有/无 `erase_if` 的容器，无者退化为"拷贝保留元素"。

## 参考

- 提案 P1209R0（采纳一致的容器擦除，源自 Library Fundamentals 2）
- cppreference: [std::erase_if](https://zh.cppreference.com/w/cpp/container/map/erase_if)
- cppreference: [std::erase（序列）](https://zh.cppreference.com/w/cpp/container/vector/erase2)
