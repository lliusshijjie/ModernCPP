# 第 37 课 · std::span（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`37_span_basics.cpp`
> 前置：第 20 课（string_view——同为"借用视图"）；第 30 课（std::byte——as_bytes 配套）

## 一、底层原理

### 1.1 span 是什么：连续数据的统一借条

`vector`、`std::array`、C 数组、`string` 的底层——**都是一段连续内存**，
但类型互不相认：函数想"读一段 int"，旧世界要么写模板 + 迭代器对，
要么重载一堆容器版本。`std::span<T>` = **指针 + 长度**的轻对象，
把"任意连续 T 序列"统一成一个类型——它是第 20 课 string_view 思想的
全类型推广。

### 1.2 借用而非拥有：三件随之而来的事

1. **拷贝即复制借条**：span 平凡可拷贝（`sizeof` = 一个指针 + 一个长度，
   64 位平台通常 16，本机 32 位目标实测 8），**按值传参**是正道，
   不需要 `const span&`
2. **不延长生命周期**：底层数据亡，span 成悬垂借条
3. **视图即实时**：通过 span 的读写直达底层，没有拷贝没有同步

### 1.3 动态长度与固定长度（extent）

`span<int>` 长度运行期携带（dynamic_extent）；`span<int, 3>` 把长度
编码进类型——长度参与编译期检查、`first<N>()` 等可用模板版。
CTAD 自动分流：从 `std::array<int,3>` 推 `span<int,3>`，
从 `vector` 推动态 `span<int>`。转换方向只能"放宽"：
`span<int,3>` → `span<int>` → `span<const int>` 可以，反向不行。

### 1.4 右值防护栏（最容易考错的一条）

从容器构造 `span<T>`（元素非 const）时，**右值容器被拒绝**（编译错）——
因为临时容器语句末销毁，可写视图必然悬垂。但 `span<const int>`
接受右值：临时在**整条调用表达式**内活着，所以"临时 vector 直接传给
`f(span<const int>)`"安全；而 `span<const int> s = std::vector<int>{...};`
**能编译但下一行就悬垂**——标准在此处只拦了可写版，只读版是诚实的坑
（示例 ① 注释 + 常见陷阱 1）。

## 二、具体语法

```cpp
int sum(std::span<const int> s);              // 只读接口：收一切连续 int

std::vector<int> v{1, 2, 3};
std::span<const int> r = v;                   // 左值容器：隐式转换
std::span<int> w = v;                         // 可写视图：共享同一份内存
sum(v);  sum(std::array<int, 3>{1, 2, 3});    // 容器直传（const 版安全）
sum(std::vector<int>{1, 2});                  // 临时 + 只读：OK
// std::span<int> s = std::vector<int>{1, 2}; // 编译错：右值 + 非 const
std::span<int, 3> fixed{v};                   // 固定 extent：只能显式
// std::span<int, 3> f2 = v;                  // 编译错：explicit 拦拷贝初始化

s.size();  s.size_bytes();  s.empty();  s.data();
s.front(); s.back();        s[i];               // 无边界检查（越界 UB）
s.first(2);  s.last(2);     s.subspan(1, 2);    // 再切视图
std::as_bytes(r);            // → span<const byte>（第 30 课 std::byte）
std::as_writable_bytes(w);   // → span<byte>
```

规则细则：

- `operator[]` 不查边界——要检查用 `.first(n)` 明示长度或自己 assert
- `first/last/subspan` 的越界参数是 UB（不是异常）
- CTAD 从 `int arr[5]` 推出 `span<int, 5>`；数组退化不再丢长度
- `span<T>` 可比较（逐元素相等，非同一内存），空 span 的 `data()` 可为空
- 容器必须**连续**（contiguous_range）：`list`/`deque`（段式）/`map` 都进不来

## 三、实际应用与最佳实践

### 3.1 场景：一个函数吃下所有"连续 int"（示例 ①/③）

`sum(span<const int>)` 同时接收 vector、std::array、C 数组、string_view
式的原始缓冲——模板膨胀与重载爆炸一并消失；处理逻辑只写一遍。

### 3.2 场景：就地修改（示例 ②）

`void bump(std::span<int> s)` 对 vector、数组、缓冲区统一"原地 +1"——
与 string_view 只读不同，span 可写是它的另一半价值。

### 3.3 最佳实践清单

1. 接口参数：只读传 `span<const T>`（按值），就地改传 `span<T>`
2. 调用方保证生命周期：**被借的数据活得比 span 久**，函数返回值尤其小心
3. 不再写 `const std::vector<T>&` 这种"只求迭代一遍"的参数——
   `span<const T>` 更宽（还兼容数组和 array）
4. 长度是接口契约时用固定 extent：`span<int, 4>`（RGB+Alpha）比
   运行期检查更早报错
5. 序列化/哈希原始内存走 `as_bytes`——与 std::byte（第 30 课）无缝衔接

## 常见陷阱

1. **`std::span<const int> s = std::vector<int>{1,2,3};` 编译通过但立即悬垂**：
   标准只拒绝"右值 + 非 const"组合；只读版要靠你自己不写出这条语句
   （临时容器直接作实参传函数是安全的，两者别混为一谈）
2. `span<T>` 绑定右值 vector 编译错——不是 bug，是救你；想传临时先存具名变量
3. `operator[]`/`subspan` 越界全是 UB：span 是"带长度的裸指针"，不是安全容器
4. `span<int,3> s = v;` 编译错（explicit）——固定 extent 拷贝初始化被禁，
   写 `std::span<int,3> s{v};`，且 v 长度不足时是 UB 不是异常
5. vector 扩容（push_back）后旧 span 悬垂——span 不追踪底层，重新获取
6. `deque` 看起来连续，但分段存储：不能构 span（编译错救你一次）

## 完整示例

见 `37_span_basics.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra "CPP20/37_std_span/37_span_basics.cpp" -o run && ./run
```

## 练习

1. 写 `avg(span<const double>)`，分别用 vector、std::array、C 数组与
   `new double[]` 指针 + 长度四种方式调用，体会"统一借条"。
2. 复现陷阱 1：`span<const int>` 绑临时 vector 能编译；注释掉解引用并
   写清原因（为何函数传参版安全、赋值版悬垂）。
3. 用 `as_bytes` 打印 `vector<int>{1,2,3}` 的前 4 个字节（十六进制），
   并解释小端序下的数值（对照第 42 课 endian）。
4. 写 `rgbadup(span<std::uint8_t, 4>)`，用 `std::array` 与 `uint8_t[4]`
   调用，验证固定 extent 的 CTAD 推导。

## 参考

- 提案 P0122R7（span）/ P0457R2（first/last/subspan 起始版）
- cppreference: [std::span](https://zh.cppreference.com/w/cpp/container/span)
- cppreference: [deduction guide（CTAD 推导）](https://zh.cppreference.com/w/cpp/container/span/deduction_guides)
