# 第 20 课 · std::string_view（C++17）

> 所需标准：**C++17** ｜ 讲解：本文件 ｜ 独立代码：`20_string_view.cpp`

## 一、底层原理

### 1.1 本质：两个词的非拥有只读视图

`string_view` 通常实现为 **(const char* 指针, size_t 长度)**，具体大小由实现决定。
它不拥有数据，也不保证视图末尾有 `'\0'`；复制视图不会复制字符或分配字符缓冲。

| 数据源 | 构造成本 | 旧写法的代价 |
|---|---|---|
| `std::string` | 零拷贝（借内部缓冲） | `const string&` 直接接，本来也零 |
| 字符串字面量 `"abc"` | 不拷贝字符；指针构造需确定长度 | 接 `const string&` 会构造临时 string，是否堆分配取决于实现与长度 |
| `char* + 长度` / 子串 | 不拷贝字符，直接记录范围 | 转成 string 需要复制字符，并可能分配 |

string_view 可以统一接收多种连续字符范围，避免仅为只读传参而构造 string。
“零拷贝”不等于所有构造都是 O(1)：只传指针时仍需要查找终止零字符。

### 1.2 O(1) 切片：解析器的视角

`string::substr` 是 O(n)（拷贝字符，并可能分配）；`string_view::substr` 只是
再抄一对指针和长度——**O(1)**。`remove_prefix/remove_suffix` 原地把视图
窗口左移/右缩。对一个"逐 token 推进"的解析器，这是从"每步一次分配"
到"全程零分配"的质变。

### 1.3 设计取舍：没有 c_str() 是故意的

`data()` 返回的内存**不保证**以 `'\0'` 结尾（视图可能切在字符串中间）。
库干脆不提供 `c_str()`——想给 C API（`fopen` 等）传参？请用 `const char*`
或 `string`。这个"缺失"是在逼你想清楚所有权与终止符，不是疏忽。

## 二、具体语法

```cpp
std::string_view sv1 = "literal";          // 字面量（零拷贝）
std::string s = "text";
std::string_view sv2 = s;                  // 借 string 的缓冲
std::string_view sv3{s.data() + 1, 2};     // 指针 + 长度

sv.size() / empty() / front() / back()
sv[i]           // 越界 UB（含 sv.size() 处）
sv.at(i)        // 越界抛 out_of_range
sv.substr(pos, len)   // O(1)，返回新视图
sv.remove_prefix(n) / remove_suffix(n)     // 原地缩窗口
sv.find("x")    // 返回下标，找不到 == npos
sv == other     // 按内容比较（不是比指针！）
std::string str(sv);                       // 显式转 string（拷贝）
```

规则细则：

- `sv → string` 的构造是 **explicit**：`std::string s = sv;` 编译不过，
  必须写 `std::string(sv)`——防止你不小心又付拷贝税
- 没有 `c_str()`；`operator[]` 在 `size()` 位置也是 UB（不像 string
  保证 `s[s.size()] == '\0'`）
- `std::hash<string_view>` 已特化——可当 `unordered_map` 的键
- `starts_with/ends_with` 要等 C++20（第 41 课预告）

## 三、实际应用与最佳实践

### 3.1 场景：解析 "key = value"（示例 ③-1）

`find('=')` 定位、`substr` 切两半、`trim`（remove_prefix/suffix 循环）去空格
——切片阶段只调整视图，不分配字符缓冲。同样的代码用 string 的 substr 写，
需要复制字符；是否发生堆分配还取决于实现与子串长度。

### 3.2 场景：统一只读字符串接口（示例 ③-2）

函数形参 `string_view`（**按值**），同时优雅接收 string / 字面量 /
零结尾字符数组——都不必复制字符；只有指针时需要确定长度。

### 3.3 最佳实践清单

1. 只读连续字符范围可以用 `string_view` 按值传递，避免复制字符
2. 长期保存视图时，必须明确底层存储的所有者，并保证地址与范围持续有效
3. 返回 `string_view` 时保证底层存储覆盖后续使用；常见情况是静态存储，
   或调用者仍持有且未使缓冲失效的输入，不能返回局部 string 的视图
4. 需要 `'\0'` 结尾（C API）：接口用 `const char*`/`string`，别用 view
5. 要修改或拥有：`string`；只看不改、当下用完：`string_view`

## 常见陷阱

1. **悬空头号案例**：`string_view sv = produce_string();` ——
   临时 string 在语句末析构，视图指向已释放内存（UB，且常常"看起来能跑"）
2. `data()` 交给只接收零结尾字符串的 C 函数，可能读到视图外的后缀，
   或在没有可访问终止符时越界；视图长度不会自动传给对方
3. 以为 `std::string s = sv;` 会隐式转换——explicit 挡住了，
   编译错是**故意的**；要拷贝就明写 `std::string(sv)`
4. `sv[sv.size()]` 是 UB（string 允许读终止符，view 不允许）
5. 成员函数隐式把 `this` 视图存下来再返回的第三方接口要警惕——
   视图的生命周期责任在你，不在接口

## 完整示例

见 `20_string_view.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++17 -Wall -Wextra "CPP17/20_std_string_view/20_string_view.cpp" -o run && ./run
```

## 练习

1. 写 `count(sv, char)` 统计字符出现次数；再写 `split(sv, delim)` 返回
   `vector<string_view>`——体会不复制字符的切片；vector 自身仍可能分配。
2. 故意写 `string_view sv = std::string("tmp");` 后再使用 sv，
   用 `-fsanitize=address` 观察悬空报告（看完即删）。
3. 给第 06 课 ConfigCache 的 `get` 加一个 `string_view` 形参重载，
   对比两种接口在传字面量时的调用成本。
4. 用 `std::hash<string_view>` 把视图当 `unordered_map` 的键做一次词频统计，
   思考：被统计的 string 必须由谁持有？

## 参考

- 提案 N3921 / P0254: string_view
- cppreference: [std::basic_string_view](https://zh.cppreference.com/w/cpp/string/basic_string_view)
- 《Effective Modern C++》无条目；C++ Core Guidelines SL.3 / GS?

## 补充阅读

[20A · 字符串视图的所有权与长度边界](../补充知识点/20_补充A_字符串视图的所有权与长度边界/20_补充A_字符串视图的所有权与长度边界.md)：进一步区分借用与快照，练习内嵌零字符及 C 接口适配。
