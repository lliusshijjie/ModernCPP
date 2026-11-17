# 第 53 课 · ranges::to 与容器构造（C++23）

> 所需标准：**C++23**（`-std=c++23`）｜ 独立代码：`53_ranges_to.cpp`
> 本机示例使用 print，需链接 -lstdc++exp；ranges::to 自身不要求该链接库。
> 前置：第 32–34 课（ranges 算法、视图与管道）

## 一、底层原理

### 1.1 从范围构造结果对象

`std::ranges::to<C>(r)` 把范围中的元素用于构造结果 C，适合把惰性查询的结果
存入容器。C 必须是符合要求的非 cv 类类型且不是 view，源需是 input_range，
还要满足相应构造/元素转换条件，不能说“任意范围、任意容器都能转”。

不少视图本身已支持随机访问或就地排序（第 34 课），只有需要拥有结果、
重复计算过贵或目标接口要求具体容器时才需要物化。

### 1.2 构造策略有先后顺序

当元素可按要求转换时，依次尝试：

1. **直接范围构造**：`C(r, args...)`，例如同类型容器的复制/移动构造。
2. **带标签的范围构造**：`C(std::from_range, r, args...)`。
3. **迭代器对构造**：满足 common_range、相应迭代器与构造要求时使用。
4. **构造后插入**：先 `C(args...)`，再以 push_back 或 insert(end, value) 插入。
   在此路径上，若源是 sized_range 且目标满足可 reserve 的容器要求，
   **会先 reserve 源的大小**。

其他路径的容器构造函数也可能预先分配；没有独立 reserve 参数不等于完全不
预留容量。对于嵌套范围，to 还可递归转换元素为目标的 value_type。

### 1.3 调用形态与类型推导

```cpp
auto a = r | std::ranges::to<std::vector>();
auto b = std::ranges::to<std::vector>(r);
auto c = r | std::ranges::to<std::vector<long>>();
```

`to<std::vector>()` 创建可供管道调用的闭包；`to<std::vector>` 是函数模板
名字，不能省掉调用括号直接拿来管道。模板模板参数版本使用相应构造与推导
规则得出完整目标类型，并不总是机械地替换一个元素类型。

## 二、具体语法

```cpp
using namespace std::views;
auto v = iota(1, 10) | filter(even) | std::ranges::to<std::vector>();
auto s = iota(1, 7) | transform(mod3) | std::ranges::to<std::set>();
auto t = iota('a', 'f') | std::ranges::to<std::string>(); // abcde

// 嵌套范围递归转成拥有字符的 string，不必先手写 stringify
auto tokens = csv | split(',') | std::ranges::to<std::vector<std::string>>();

// 显式目标元素类型，去掉 map 的键 const：
auto pairs = freq | std::ranges::to<std::vector<std::pair<std::string,int>>>();
```

规则细则：

- 额外实参会转发给选定的构造路径，如比较器或分配器；可写
  `to<std::set<int,std::greater<int>>>(std::greater<int>{})` 创建闭包。
- 物化立即消费所需范围；对不会终止的无界源，先安排可终止的消费方式。
- 转换可复制或移动，取决于构造路径和源引用类别。把容器 std::move 后直接
  转为相同类型，可能走移动构造；对元素级移动可使用 views::as_rvalue，
  不能假设任意右值视图都会把其元素变成右值引用。
- string、vector<string> 等拥有型结果可拥有复制后的数据；vector<string_view>、
  指针和 reference_wrapper 等结果仍借用源。to 不提供通用深拷贝或延寿保证。
- 元素构造、容器分配和转换可抛异常，性能取决于实际路径。

## 三、实际应用与最佳实践

### 3.1 场景：split 的递归物化

对字符子区间明确指定 vector<string>，to 可递归构造每个 string。
若只写 to<vector>()，推导出的元素可能仍是借用源的子区间，不等同于字符串副本。

### 3.2 场景：词频表排序

map 的 value_type 为 pair<const K,V>；直接 to<vector>() 通常保留该类型，
其 const 键不满足排序所需的赋值/交换要求。可先 transform 成普通 pair，
也可显式指定 `to<vector<pair<K,V>>>()`，不必一律增加 transform。

### 3.3 最佳实践清单

1. 明确目标类型与所有权；有嵌套范围或借用元素时检查推导结果。
2. 只遍历一次且不需要保存时保留视图；需要结果时使用 to。
3. 性能敏感时检查实际构造路径、sized_range 能力及分配次数。
4. 自定义容器可提供 from_range 构造或合适插入接口，并满足语义约束。
5. 关联容器会按自身规则排序、去重或处理重复键，不保证逐元素一一保留。

## 常见陷阱

1. 忘记闭包末尾括号，或把普通 lambda 自动当作范围适配器闭包。
2. 标准范围管道不会为任意 lambda 提供 operator|；自定义适配器可按规则
   继承 range_adaptor_closure。不能由此断言语言层面所有 operator| 都只能用于它。
3. iota(a,b) 是半开区间，iota('a','f') 不包含 f。
4. map 转 vector 后元素仍含 const 键，需调整目标元素类型再排序。
5. 没有 reserve 参数不等于没有自动容量预留；filter 等通常又不保留 sized_range。
6. 转成 string_view 容器不拷贝字符；销毁源字符串后仍可能悬垂。

## 完整示例

```bash
g++ -std=c++23 -Wall -Wextra "CPP23/53_ranges_to/53_ranges_to.cpp" -o run -lstdc++exp && ./run
```

## 练习

1. 比较 map 到 vector 的推导类型与显式 pair<K,V> 目标类型，并测试排序约束。
2. 用 split | filter | to<vector<string>> 过滤空 token，说明结果为何拥有字符。
3. 用带计数的分配器或容器记录分配/reserve 次数；最终 capacity() 本身不能
   反推出经历过几次增长。比较有 sized_range 与 filter 后失去该能力的输入。
4. 为 SmallVec<T,N> 提供 from_range 构造，使用 `to<SmallVec<int,8>>(r)`；
   N 是非类型参数，不能直接传给只接受类型参数包的 to<模板>() 版本，
   也不能期望从任意源自动推导出 N。

## 参考

- [C++23 草案：ranges::to 的构造顺序与 reserve](https://timsong-cpp.github.io/cppwp/n4950/range.utility.conv.to)
- [C++23 草案：范围递归转换](https://timsong-cpp.github.io/cppwp/n4950/range.utility.conv)
- [C++23 草案：范围适配器闭包](https://timsong-cpp.github.io/cppwp/n4950/range.adaptor.object)
- 提案 P1206R7（ranges::to 与 from_range）
