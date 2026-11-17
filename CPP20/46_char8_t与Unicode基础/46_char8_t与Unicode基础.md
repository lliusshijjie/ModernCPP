# 第 46 课 · char8_t 与 Unicode 基础（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`46_char8_unicode.cpp`
> 前置：第 42 课（<bit>——无符号位运算）；第 20 课（string_view 的字节观）

## 一、底层原理

### 1.1 char8_t：给 UTF-8 一个专属类型

C++17 里 `u8"x"` 的类型是 `const char[]`——UTF-8 字节与"普通窄字符串"
类型上无法区分，`std::string` 与"UTF-8 字符串"混着用，编码错配
静默发生。C++20 引入 **char8_t**（unsigned char 的独立类型）：

- `u8"你好"` → `const char8_t[]`；`u8'a'` → `char8_t`
- 与 char / unsigned char 之间**无隐式转换**（要显式 cast）
- `std::u8string` / `std::u8string_view` 配套存在

**类型即协议**："这串字节按 UTF-8 解释"从注释约定升级为类型事实。

### 1.2 为什么要新类型：char 的三宗罪

1. **符号不定**：char 是否有符号由实现决定（x86/x64 有符号）——
   `char c = 0xE4;` 读出 **-28**（示例 ①），进位运算/比较全盘出错；
   char8_t **保证无符号**（0xE4 → 228）
2. **语义过载**：char 同时当"字符"“原生字节”“窄字符串元素”三个角色；
   UTF-8 存进去后 `s[0]` 是"半个字符"的秘密没人管
3. **重载歧义**：C++17 中 `u8""` 与 `""` 同型，`f(const char*)` 会
   意外接住 UTF-8 字面量；C++20 起编译错拦住

### 1.3 Unicode 三层概念（必修的常识课）

| 层 | 单位 | 例子 |
|---|---|---|
| **字节**（byte/code unit） | UTF-8 的 8 位 | `你好` = 6 字节 |
| **码点**（code point） | U+XXXX 抽象字符 | `你好` = 2 码点（U+4F60、U+597D） |
| **字素**（grapheme cluster） | 用户感知的"一个字" | emoji 👨‍👩‍👧 = 3 码点 8 字节，**1 字素** |

`str.size()` 数的是**字节**；"字符串里有几个字"可能要数到字素——
后者标准库无能为力，上 ICU/uni-algo 等库。

### 1.4 UTF-8 编码速览（前缀字节法）

```
字节模式      长度   码点位数
0xxxxxxx      1     7 位（ASCII）
110xxxxx      2     11 位
1110xxxx      3     16 位（含全部 CJK 汉字）
11110xxx      4     21 位（含 emoji）
10xxxxxx      后续字节（不可能是开头）
```

由首字节判断当前码点长度（示例 ③ 的 `seq_len`）、数非
`10xxxxxx` 开头的字节得码点数（示例 ②）——这是不依赖库的
最小可用 UTF-8 工具箱。

## 二、具体语法

```cpp
const char8_t s[] = u8"你好";     // 数组声明（auto 会退化成指针！）
sizeof(s);                        // 7（6 字节 + NUL）
std::u8string u{u8"cpp20"};
std::u8string_view uv{u8"a你好b"};

static_cast<char>(s[0]);          // 显式转 char（可能变负数——转 unsigned char 才安全）
static_cast<unsigned>(s[0]);      // 228：位运算前先无符号化

// 以下全部编译错（C++20 的"隔离墙"）：
// std::cout << s;                 // 无 char8_t 的 <<
// std::string str = u;            // u8string -> string 无隐式转换
// const char* p = u8"x";          // C++17 合法，C++20 拒绝
```

规则细则：

- char8_t 与 unsigned char 大小同为 1，但**是不同类型**：
  重载/模板推导严格区分
- u8 字面量含**非 ASCII 即编译错**的执行字符集问题不存在——
  UTF-8 字面量按源码原样编码
- `\u` / `\U` 通用字符名：`u8"你"` 与 `u8"你"` 等价
- 转换函数 `std::mbrtoc8 / c8rtomb`（<cwchar>）存在于标准，
  但实现稀少（含本机）——实战转换多为逐字节 `static_cast`
- `std::filesystem::u8path` 在 C++20 **废弃**：改用
  `path(std::u8string)` 构造

## 三、实际应用与最佳实践

### 3.1 场景：边界明确的最小 UTF-8 处理（示例 ②/③）

计数、截断到码点边界、按前缀判长度——协议解析、日志截断这类
"不敢引入 ICU"的场景，前缀字节法 20 行搞定。注意：**截断必须落在
码点边界**，否则尾部出现半个字符（替换字符问号的来源）。

### 3.2 场景：与既有 char 世界的桥接（示例 ②）

IO/文件系统仍是 char 的天下——`to_ascii_hex` 这类"显式逐字节转换"
是合规通道；把转换点集中在一处（边界函数），内部保持 u8 类型，
是过渡期的标准架构。

### 3.3 最佳实践清单

1. 字节运算前一律 `static_cast<unsigned>`（或直接用 char8_t）——
   `signed char` 进位运算是国际错误大本营
2. `auto` 接字面量会**退化成指针**：要数组写 `const char8_t s[] = ...`
3. "长度"先问清量什么：字节（size）/ 码点（前缀法）/ 字素（上库）
4. 源码内嵌中文示例输出走 hex 或转义——控制台编码不可依赖
   （本课程示例一律 ASCII 输出的原因）
5. 重文本处理（排序、正规化、字素切分）直接上库——
   标准库只承诺"字节不弄丢"

## 常见陷阱

1. **`std::cout << u8str` 编译错**——不是缺头文件，是生态故意留白；
   转换后输出（且当心目标终端编码）
2. `char c = 0xE4; (c & 0xC0)` 结果带符号污染——比较/掩码全错；
   用 char8_t 或 unsigned char（示例 ① 的 -28 vs 228）
3. `s.substr(0, 4)` 截断在多字节字符中间——尾部半个字符；
   按 `seq_len` 回退到边界再截
4. 以为 `u8string` 能进文件/正则/格式化：`format("{}",
   u8string_view)` 不接受（格式化器只认 char 系）——
   桥接层还是 char
5. emoji/组合字符："字数对不上"十有八九是字素 vs 码点之差——
   标准库没有字素切分，别手写
6. Windows 源码文件编码坑：MSVC 需要 `/utf-8` 才把源与执行字符集
   都按 UTF-8 处理；g++ 默认 UTF-8（本课环境 OK，跨平台要知道）

## 完整示例

见 `46_char8_unicode.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra "CPP20/46_char8_t与Unicode基础/46_char8_unicode.cpp" -o run && ./run
```

## 练习

1. 写 `code_point_at(u8string_view, size_t n)`：返回第 n 个码点的
   起始字节下标；对 `u8"a你好b"` 求 n=2（答案：4）。
2. 用前缀字节法写"按码点截断"：`truncate(s, max_cps)`——
   对 `u8"你好"` 截 1 个码点应得前 3 字节。
3. 验证字素陷阱：`u8"\U0001F468‍\U0001F469‍\U0001F467"`
   （家庭 emoji）打印字节数、码点数，与"看起来有几个字"对比。
4. 写 char8_t ↔ char 的双向桥接函数，并在注释里论证：
   为什么这个转换点应该集中在一个文件里。

## 参考

- 提案 P0482R6（char8_t）— 同时废弃了 u8path 的提案部分
- cppreference: [char8_t](https://zh.cppreference.com/w/cpp/language/types#char8_t)
- UTF-8 维基百科（前缀字节表）：编码规则一节
- cppreference: [std::u8string](https://zh.cppreference.com/w/cpp/string/basic_string)
