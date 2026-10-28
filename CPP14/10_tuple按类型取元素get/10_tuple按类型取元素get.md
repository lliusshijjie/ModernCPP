# 第 10 课 · std::tuple 按类型取元素 get\<T\>（C++14）

> 所需标准：C++14 ｜ 讲解：本文件 ｜ 独立代码：`10_tuple_get_by_type.cpp`

## 一、底层原理

### 1.1 get\<I\> 与 get\<T\>：都发生在编译期

C++11 只能按下标取元素：`get<0>(tup)`——下标是魔法数字，元素一调序全崩。
C++14 补上按类型取：`get<T>(tup)`。**编译器视角**：两者都是编译期定址——

| | get\<I\> | get\<T\>（C++14） |
|---|---|---|
| 定址依据 | 字面量下标 | 在类型列表里**编译期查找** T 的下标 |
| 运行期开销 | 直接成员偏移 | **完全相同**（查找不发生在运行期） |
| 失败模式 | 下标越界编译错 | T 出现 0 次：编译错；出现 2 次：歧义编译错 |

没有 RTTI、没有哈希表——类型列表的查找在重载决议/模板特化阶段完成，
"按类型访问"和"按下标访问"生成完全相同的机器码。

### 1.2 "恰好一次"约束是特性不是缺陷

`tuple<int, int, string>` 上 `get<int>` 直接编译错误——因为"按类型寻址"
的前提是类型唯一。想要两个 string 字段？给它们各自的语义身份（见 ③ 的
强类型键模式），类型重新变回唯一的——这恰好逼出更好的设计。

### 1.3 返回类别跟随 tuple 的值类别

`get` 是完善的引用接口：左值 tuple → `T&`，const → `const T&`，右值 → `T&&`
（可以 move 出去）。零拷贝访问，与直接访问成员等价。

## 二、具体语法

```cpp
std::get<T>(tup)        // 按类型（C++14，T 在元组中必须恰好出现一次）
std::get<I>(tup)        // 按下标（C++11）
std::get<T>(pair)       // pair 也支持按类型（两元素类型必须不同）
std::get<I>(arr)        // std::array 支持按下标（编译期边界检查）
std::tie(a, b) = tup;   // 解包到既有变量（std::ignore 跳过不想要的）
```

规则细则：

- 元素访问返回引用类别跟随 tuple 本身（见 1.3），`std::move(tup)` 后可搬空
- 元信息：`tuple_size<V>::value`、`tuple_element<I, V>::type`
- 自定义类型想让 `get<T>` 生效需要按 tuple 接口特化——通常直接用 tuple

## 三、实际应用与最佳实践

### 3.1 场景：函数返回多个值，且可读性不能丢

`tuple<Name, Salary, Level>` + `get<Name>(r)` 比下标自文档化；
再配合**强类型键**（第 ③ 段的 Name/Age/Mail 薄包装），同底层类型的字段
也能按语义取——这是 tuple 当"记录类型"用的标准姿势。

### 3.2 场景：tie 解包替代出参

```cpp
int lo, hi;
std::tie(lo, hi) = minmax_range(v);   // 对比 minmax_range(v, lo, hi) 出参风格
```

### 3.3 最佳实践清单

1. 返回多值优先 tuple/pair + `tie`/C++17 结构化绑定（第 11 课的前菜）
2. 元组字段靠类型区分时，给字段配强类型薄包装——类型即键，语义即名
3. 访问一律 `get<T>`/`get<I>`，别用 `tuple_element` 手写运行期访问
4. 元组超过 4-5 个字段就换 struct——tuple 适合轻量聚合，不是万能容器
5. 需要 move 出元素：`std::get<T>(std::move(tup))`

## 常见陷阱

1. **同类型两字段**：`tuple<int, int>` 上 `get<int>` 是歧义编译错误——
   用强类型包装（③-1）而不是退化回魔法下标
2. `get<T>` 的 T 要写**精确类型**：`tuple<int>` 上 `get<int&&>`/`get<const int>`
   都找不到（匹配的是存储类型 int）
3. 悬空的 tie：`std::tie` 解包临时元组后，绑定的引用在语句结束即失效——
   tie 只接左值，右值请用 `auto [a, b] = ...`（C++17）或 `auto` 接
4. `std::get<I>(arr)` 越界是编译错（好事），但运行期下标请用 `arr[i]`
5. tuple 嵌套 tuple 时 `get<tuple<...>>` 类型必须一字不差——
   类型列表匹配不玩"能转换就行"

## 完整示例

见 `10_tuple_get_by_type.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++14 -Wall -Wextra "CPP14/10_tuple按类型取元素get/10_tuple_get_by_type.cpp" -o run && ./run
```

## 练习

1. 写 `constexpr` 下验证：对 `tuple<int, char>` 分别取 `get<int>` 与
   `get<char>`，用 `static_assert` 断言两个返回类型的差异。
2. 实现第 ③ 段的 Person，给 Age 加 `operator<<`，让整个 Person 能逐字段打印。
3. 用 `std::tie` + `std::ignore` 解包三元组的第 2、3 个字段。
4. 故意写 `tuple<int, int>` 的 `get<int>` 编译一次，把歧义错误信息存档
   ——以后见到就能秒懂。

## 参考

- cppreference: [std::get(tuple)](https://zh.cppreference.com/w/cpp/utility/tuple/get)
- 《Effective Modern C++》Item 47 相关讨论（traits 与 tuple 元信息）
- cppreference: [std::tie](https://zh.cppreference.com/w/cpp/utility/tuple/tie)
