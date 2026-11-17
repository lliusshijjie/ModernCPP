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
统一成**一行标准件**：返回删除个数，保留未删元素的相对顺序；
有序关联容器维持其排序，无序关联容器不提供排序。

### 1.2 为什么是自由函数而不是成员？

map 的 `erase(key)` 只删**一个键**；"按谓词删一批"语义需要新名字。
标准选择放进 `std::` 命名空间做**自由函数重载**（Library Fundamentals 2
移植），好处：泛型代码里对受支持的标准容器可用同一句法——
连 `std::string` 都有份（按字符谓词清串）。

### 1.3 性能面：与惯用法等价、与成员互补

vector 版与 remove-erase 惯用法等价，线性检查并移动保留元素，
标准不保证常数开销更好。map 版封装了遍历中 erase 并接收后继迭代器的循环。
调用者仍须遵守失效规则：vector 压缩时会移动赋值，之前保存的引用可能
已对应不同的值；最后 erase 尾部使新尾及之后的迭代器/引用失效。
map/set 被删元素的迭代器/引用失效，其他仍有效。注意它与单点成员各司其职：
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
// unordered_* 也有 erase_if；std::erase 仅提供给受支持的序列容器
```

规则细则：

- `std::erase(v, value)` 对 vector 是**删除全部等值元素**——
  vector 的成员 erase 接收迭代器或区间，不接受待删除值。
  map/set 按键删则用成员 erase(key)，不存在通用的 std::erase(map,key)
- map 元素类型是 `pair<const K, V>`；推荐 `[](const auto& kv)` 避免复制，
  用 kv.first/kv.second 或 `const auto& [key,value] = kv` 读取
- 返回类型是容器 `size_type`（删除个数）——日志、统计直接用
- 序列容器保留相对顺序；有序关联容器仍保持排序，无序容器仍不保证排序
- 谓词里**不要修改容器本身**（既改又删是 UB 级骚操作）
- 空容器返回 0；全删光时返回调用前的 size()，调用后 size() 为 0
- 若待删值引用容器内部元素，remove 的移动可能改变该引用所指的值；
  例如 std::erase(v, v.front()) 应先复制目标值，再传给 erase

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
4. 泛型接口先检测是否存在合适的 std::erase_if 重载；
   array、原生数组、span 和自定义容器不一定支持
5. 删除条件依赖位置/索引（删第偶数个）erase_if 不合适——
   它只看元素值

## 常见陷阱

1. **以为 `std::erase(v, x)` 删第一个**——它删**全部**匹配；
   只删第一个要先 `std::find` 再成员 `erase`
2. map<string,int> 谓词接收 `const pair<string,int>&` 不一定报错：
   可经转换构造临时 pair，再绑定 const 引用，但会额外复制。
   键不可复制时可能失败；用 const auto& 或准确的 value_type 更合适
3. 旧习难改：remove-erase 忘了 erase 那半截——现在整句换成
   `std::erase_if` 就不存在忘不忘的问题
4. 在谓词里**修改正在被遍历的容器**（push_back/erase）——UB；
   想边删边补就分两轮
5. `std::array` / 原生数组没有此接口：定长容器不存在"擦除后变短"，
   编译错是正确行为
6. 以为 erase_if 后"容量缩了"——vector 只缩 size 不缩 capacity；
   shrink_to_fit 只是非强制的容量收缩请求，不保证释放；
   若确需替换存储，另建容器并按分配器规则交换/移动

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

- [C++20 草案：vector 的统一擦除](https://timsong-cpp.github.io/cppwp/n4861/vector.erasure)
- [C++20 草案：map 的 erase_if 等价循环](https://timsong-cpp.github.io/cppwp/n4861/map.erasure)
- 提案 P1209R0（采纳一致的容器擦除，源自 Library Fundamentals 2）
- cppreference: [std::erase_if](https://zh.cppreference.com/w/cpp/container/map/erase_if)
- cppreference: [std::erase（序列）](https://zh.cppreference.com/w/cpp/container/vector/erase2)
