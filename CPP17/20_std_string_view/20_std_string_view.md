# 第 20 课 · std::string_view（C++17）

> 所需标准：**C++17** ｜ 讲解：本文件 ｜ 独立代码：`20_string_view.cpp`

## 一、底层原理

### 1.1 本质：两个词的非拥有只读视图

`string_view` = **(const char* 指针, size_t 长度)**，16 字节，不拥有数据、
不保证结尾有 `'\0'`。传参时按值拷贝这两个词即可——没有引用计数、没有堆。

| 数据源 | 构造成本 | 旧写法的代价 |
|---|---|---|
| `std::string` | 零拷贝（借内部缓冲） | `const string&` 直接接，本来也零 |
| 字符串字面量 `"abc"` | 零拷贝 | **接 `const string&` 会构造临时 string——堆分配！** |
| `char* + 长度` / 子串 | 零拷贝 | 只能先拼一个新 string——又是一次分配 |

第三行就是 string_view 存在的最大理由：`f("hello")` 这种最普通的调用，
在 `const string&` 接口下悄悄付了一次堆分配的税。

### 1.2 O(1) 切片：解析器的视角

`string::substr` 是 O(n)（分配 + 拷贝）；`string_view::substr` 只是
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
——全程只操作 16 字节的视图，零分配。同样的代码用 string 写，
每处理一行至少两次堆分配。

### 3.2 场景：统一只读字符串接口（示例 ③-2）

函数形参 `string_view`（**按值**），同时优雅接收 string / 字面量 /
字符数组——三种调用方零转换成本，接口再也不用写三个重载。

### 3.3 最佳实践清单

1. 只读字符串参数：`string_view` **按值**传（两个词，比传引用还划算）
2. **绝不长期持有**：成员变量、返回值、跨调用存放都是悬空预约——
   视图是"借阅证"，不是产权证
3. 返回 `string_view` 只允许两种情况：视图指向**参数**（活得比自己久）
   或**静态存储**
4. 需要 `'\0'` 结尾（C API）：接口用 `const char*`/`string`，别用 view
5. 要修改或拥有：`string`；只看不改、当下用完：`string_view`

## 常见陷阱

1. **悬空头号案例**：`string_view sv = produce_string();` ——
   临时 string 在语句末析构，视图指向已释放内存（UB，且常常"看起来能跑"）
2. `data()` 交给期望 NUL 结尾的 C 函数 → 越界读；视图切在中间时必炸
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
   `vector<string_view>`——体会全程零分配。
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
