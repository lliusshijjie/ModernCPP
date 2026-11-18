# 第 57 课 · std::flat_map / flat_set（C++23）

> 标准：C++23；代码：`57_flat_containers.cpp`。
> 本机 GCC 15.1.0/libstdc++ 已提供 `<flat_map>` 和 `<flat_set>`，不是提前实现未来标准。
> 示例使用 print，按本机工具链链接 `-lstdc++exp`。
> 前置：第 30 课节点句柄、第 32 课迭代器概念。

## 一、底层原理

### 1.1 有序的序列容器适配器

flat_map/flat_set 用序列容器保存有序键。默认底层为 vector，也可使用满足要求的 deque；
因此“扁平”不保证任何底层选择都物理连续。map/set 常见实现是平衡树，标准不指定必须是红黑树，
也不规定每个节点需要几次内存分配。

flat_map 的键和值是**两个平行容器**，这是标准设计，不只是 libstdc++ 的实现技巧。
排序、去重和修改必须保持键值对应与键序不变量。

| 方面 | map/set | flat_map/flat_set |
|---|---|---|
| 查找 | 对数复杂度 | 对数复杂度，默认 vector 可有局部性优势 |
| 单元素插入/删除 | 通常对数复杂度；位置擦除等另有规则 | 线性复杂度，可能搬移元素 |
| 遍历 | 双向迭代 | 满足现代随机访问迭代器概念 |
| 元素稳定性 | 插入通常不使既有迭代器失效，擦除失效被擦除者 | 取决于底层容器，不能沿用树容器的保证 |
| 节点句柄 | 支持 | 不支持；另有整批 extract/replace |
| 默认存储 | 常见节点存储 | set 一条 vector，map 两条 vector |

性能取决于元素大小、比较成本、访问模式与分配器。读多写少、批量构建是候选场景，
“查找必然更快”“写多必然选树”都应通过具体负载测量，而不是由复杂度单独推导。

### 1.2 flat_map 的代理引用

`flat_map<K,V>::value_type` 是 `pair<K,V>`，`reference` 却是 `pair<const K&,V&>`。
解引用迭代器得到代理对象，因此通常不能写 `for (auto& [k,v] : fm)`。
可以用 `auto&& [k,v]` 或 `const auto& [k,v]`；键只读，值可以修改。
注意：对非 const fm，即使代理绑定为 `const auto&`，其 `V&` 成员依旧指向可写值。
需要只读遍历时应让容器本身为 const，例如遍历 `std::as_const(fm)`。

### 1.3 随机访问不等于可排序

二者满足 `random_access_iterator`，但 flat_map 的代理引用不满足旧式 ForwardIterator 的真引用要求，
旧式 `iterator_category` 可能只有 input 标签；本机 flat_set 标签为 random access。
应区分标准保证和本机标签，不把所有实现的旧标签写死。

键不可通过迭代器修改，所以 `ranges::sort(fm)` 或 `ranges::sort(fs)` 不合法。
满足随机访问只说明定位能力，还必须满足算法要求的可写、可交换等约束。

## 二、具体语法

```cpp
std::flat_map<std::string, int> fm;
fm["apple"] = 1;
fm.insert_or_assign("apple", 2);
fm.try_emplace("banana", 3);
fm.contains("apple");
std::flat_set<int> fs{5, 1, 3, 1};               // 1,3,5

std::vector<std::pair<int, std::string>> raw{{3,"c"}, {1,"a"}, {2,"b"}};
std::flat_map<int, std::string> built(raw.begin(), raw.end());
// 普通区间构造可处理未排序输入；这里复制左值元素，不是直接接管 raw 的存储。

std::flat_set<int> ready(std::sorted_unique, std::vector<int>{1,3,5});
// sorted_unique 是调用方承诺：按当前比较器有序、无等价重复键。

auto parts = std::move(built).extract();         // 取走所有 keys/values，built 变空
built.replace(std::move(parts.keys), std::move(parts.values));
// replace 前提：长度相等、keys 按比较器有序且唯一。
```

`flat_map::extract() &&` 返回包含 keys 与 values 的 `containers`，
`flat_set::extract() &&` 返回整个底层容器；两者都不是 `map.extract(key)` 的单节点提取。
没有 map/set 那种节点句柄式 merge。标准提供 `std::erase_if`。
本机 libstdc++ 15.1 将相应函数实现为隐藏友元，限定名 `std::erase_if(fm, pred)` 查找失败；
可用非限定调用 `erase_if(fm, pred)` 经 ADL 找到。本机内部还把 zip 的 tuple 传给谓词，
而标准描述的是 `pair<const K&, const V&>`；可写成接受 const 引用并用 `std::get<0>(entry)`
访问键的泛型谓词，兼容两种形态。示例演示这一写法，需包含 `<tuple>`。
这些是本机实现边界，不是标准删除了该接口。

键的顺序和等价性由严格弱序比较器决定，不一定是数值升序或 `operator==`。
`lower_bound(k)` 按比较器找第一个不先于 k 的元素；`equal_range` 返回半开区间，
末端可能是 `end()`，不能无条件解引用。透明比较器可支持异构查找。

## 三、实际应用与最佳实践

### 3.1 批量构建

普通区间构造负责排序与去重，适合批量输入；已经排序且唯一的数据可使用 `sorted_unique` 接口。
未满足承诺却加标签是 UB，不会自动检查后替你排序。重复键保留哪条值不宜依赖普通排序去重过程；
需要业务明确的覆盖策略时先明确处理，或使用 `insert_or_assign`。

### 3.2 修改与失效

默认 vector 底层在重分配时，相关迭代器和引用可能全部失效；即使不重分配，
插入会使插入点及其后位置失效，擦除会使擦除点及其后位置失效。
不能只写“修改点之后失效”，也不能把该表直接套到 deque 底层。
修改已存在键的 mapped value 与插入新键还应分别分析。

flat 容器必须保持不变量；异常情况下可能通过清空内容恢复不变量，
不保证所有修改都具有“失败后元素完全不变”的强保证。底层容器、元素类型和具体操作都影响异常行为。

## 常见陷阱

1. 常用查询相似不等于接口完全对齐；迁移还要核对引用、失效、extract、分配器和异常保证。
2. flat_map 的代理不是内存中真实存在的 `pair<K,V>` 数组，不能取连续 pair 数据交给 C API。
3. 默认 vector 底层也没有通用的 flat_map::reserve 成员；需要时通过准备底层容器等途径设计构建过程。
4. 不要实际解引用已经失效的迭代器来观察结果；按规则重取，并用调试迭代器检查误用。
5. unordered_map 的查找是平均常数、最坏线性；不能无条件写成 O(1) 或断言内存一定更大。
6. 键必须保持排序。需要改键时按删除/重新插入等合法接口处理，而不是强转去 const。

## 完整示例

```bash
g++ -std=c++23 -Wall -Wextra "CPP23/57_flat_map与flat_set/57_flat_containers.cpp" -o run -lstdc++exp && ./run
```

## 练习

1. 分别计时批量构建、随机查询、有序扫描和中段修改，比较 map 与 flat_map；避免把不同构建方式混为同一测量。
2. 检查代理的 reference 类型，验证可改 value、不可改 key，保留 `auto&` 为预期编译失败。
3. 用 `lower_bound(lo)` 与 `lower_bound(hi)` 实现半开分数段查询；重复分数考虑 flat_multimap。
4. 验证整批 extract/replace 后键值对应，再将底层替换成 deque，核对失效规则和连续性差异。

## 参考

- [C++23 草案：flat_map 设计、代理和修改](https://timsong-cpp.github.io/cppwp/n4950/flat.map)
- [C++23 草案：flat_map 构造](https://timsong-cpp.github.io/cppwp/n4950/flat.map.cons)
- [C++23 草案：flat_set](https://timsong-cpp.github.io/cppwp/n4950/flat.set)
