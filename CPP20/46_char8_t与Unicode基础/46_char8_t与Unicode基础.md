# 第 46 课 · char8_t 与 Unicode 基础（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`46_char8_unicode.cpp`
> 前置：第 42 课（<bit>——无符号位运算）；第 20 课（string_view 的字节观）

## 一、底层原理

### 1.1 char8_t：给 UTF-8 一个专属类型

C++17 里 `u8"x"` 的类型是 `const char[]`——UTF-8 字节与"普通窄字符串"
类型上无法区分，`std::string` 与"UTF-8 字符串"混着用，编码错配
静默发生。C++20 引入 **char8_t**（以 unsigned char 为底层类型的独立整数类型）：

- `u8"你好"` → `const char8_t[]`；`u8'a'` → `char8_t`
- 单个数值可与其他整数类型隐式转换；但 char8_t* 与 char* 不隐式互转，
  u8string 与 string 也不会隐式互转，重载和模板推导能区分这些类型
- `std::u8string` / `std::u8string_view` 配套存在

该类型表达 UTF-8 代码单元的意图，但不会自动验证内容；
手工填入 u8string 的序列仍可能不是合法 UTF-8。

### 1.2 为什么要新类型：char 的三宗罪

1. **符号不定**：char 是否有符号由实现及编译选项决定，不由 x86/x64
   架构单独决定。本机默认 8 位 signed char 模式下，转入 0xE4 后读成 -28；
   char8_t 保证无符号。涉及移位、范围比较时先转换为 unsigned char 等类型
2. **语义过载**：char 同时当"字符"“原生字节”“窄字符串元素”三个角色；
   UTF-8 存进去后 `s[0]` 是"半个字符"的秘密没人管
3. **重载歧义**：C++17 中 `u8""` 与 `""` 同型，`f(const char*)` 会
   意外接住 UTF-8 字面量；C++20 起编译错拦住

### 1.3 Unicode 三层概念（必修的常识课）

| 层 | 单位 | 例子 |
|---|---|---|
| **代码单元**（code unit） | UTF-8 的 8 位单元（通常为一个字节） | `你好` = 6 字节 |
| **码点**（code point） | 编码空间中的数值位置（不一定已分配给字符） | `你好` = 2 码点（U+4F60、U+597D） |
| **字素**（grapheme cluster） | 用户感知的"一个字" | emoji 👨‍👩‍👧 = 5 码点、18 个 UTF-8 字节（含两个 U+200D），通常为 **1 字素簇** |

`string/u8string` 的 size() 数代码单元，UTF-8 下通常就是字节；
u16string/u32string 的 size() 也数各自代码单元，不能一概当字节数。"字符串里有几个字"可能要数到字素——
C++20 标准库没有通用字素分段接口，完整文本处理可用 ICU 等 Unicode 库。

### 1.4 UTF-8 编码速览（前缀字节法）

```
字节模式      长度   码点位数
0xxxxxxx      1     7 位（ASCII）
110xxxxx      2     11 位
1110xxxx      3     16 位（BMP 中的汉字等，不含代理项）
11110xxx      4     21 位（含 emoji）
10xxxxxx      后续字节（不可能是开头）
```

由首字节判断当前码点长度（示例 ③ 的 `seq_len`）、数非
`10xxxxxx` 开头的字节得码点数（示例 ②）。这是对**已验证合法 UTF-8**
的简单遍历方法，不是验证器：还要检查续字节、
截断、过长编码、代理项和超过 U+10FFFF 等非法输入。补充平面的汉字也需 4 字节。

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
- 编译器先按输入字符集解码源码，再把 u8 字面量编码成 UTF-8；
  源文件编码仍须配置正确，不是原始源文件字节直接照抄
- `\u` / `\U` 通用字符名：`u8"\u4F60"` 与正确解码的 `u8"你"` 等价
- `std::mbrtoc8 / std::c8rtomb` 声明在 **<cuchar>**，在当前 C locale 的
  多字节编码与 UTF-8 代码单元之间做有状态转换；并非无条件的 UTF-8 字节拷贝。
  实现支持需检查；单纯 char/char8_t 代码单元桥接也不等于编码转换
- `std::filesystem::u8path` 在 C++20 **废弃**：改用
  `path(std::u8string)` 构造

## 三、实际应用与最佳实践

### 3.1 场景：边界明确的最小 UTF-8 处理（示例 ②/③）

协议解析、日志截断等场景中，输入已验证为合法 UTF-8 时可用前缀法
计数或遍历；外部任意字节需先验证或
明确定义错误处理。截断要落在码点边界；若要保留用户感知字符，还需
字素簇边界，不能只保证单个码点完整。

### 3.2 场景：与既有 char 世界的桥接（示例 ②）

标准 ostream 没有直接打印 char8_t 字符串的插入器，但
filesystem::path 支持 u8string；`to_ascii_hex` 是把代码单元编码为 ASCII
十六进制文本，并非把原 UTF-8 转码到 char 字符串。把转换点集中在一处（边界函数），内部保持 u8 类型，
可减少编码假设散落各处。按值复制代码单元不等于编码验证/转码；
char 可以读取对象表示，但 char8_t 没有通用别名访问豁免，不能把普通
char 缓冲强转成 char8_t* 后就任意通过该指针访问。

### 3.3 最佳实践清单

1. 若输入可能是 signed char，先转 unsigned char，再做数值提升和位操作；
   直接把负 char 转 unsigned 可能保留整型宽度上的符号扩展结果
2. `auto` 接字面量会**退化成指针**：要数组写 `const char8_t s[] = ...`
3. "长度"先问清量什么：字节（size）/ 码点（前缀法）/ 字素（上库）
4. 源码内嵌中文示例输出走 hex 或转义——控制台编码不可依赖
   （本课程示例一律 ASCII 输出的原因）
5. 重文本处理（排序、正规化、字素切分）直接上库——
   标准库只承诺"字节不弄丢"

## 常见陷阱

1. **`std::cout << u8str` 编译错**——不是缺头文件，是生态故意留白；
   转换后输出（且当心目标终端编码）
2. 有符号 char 提升可能影响范围比较/移位，但 `(c & 0xC0)` 这个掩码
   本身不一定错；不要把符号差异概括成所有位运算都错。先转 unsigned char
   能更清楚地表达按代码单元处理
3. `s.substr(0, 4)` 截断在多字节字符中间——尾部半个字符；
   按 `seq_len` 回退到边界再截
4. C++20 的标准 char/wchar_t 格式化器不直接接受 u8string_view，
   filesystem::path 却支持 u8string；不同组件的支持情况要分别判断
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
   对 `u8"你好"` 截 1 个码点应得前 3 字节；明确合法输入前提，
   并说明截断到码点边界为何仍可能拆开一个字素簇。
3. 验证字素陷阱：`u8"\U0001F468\u200D\U0001F469\u200D\U0001F467"`
   （家庭 emoji）打印字节数、码点数，与"看起来有几个字"对比。
4. 写 char8_t ↔ char 的双向桥接函数，并在注释里论证：
   为什么这个转换点应该集中在一个文件里。

## 参考

- 提案 P0482R6（char8_t）— 同时废弃了 u8path 的提案部分
- cppreference: [char8_t](https://zh.cppreference.com/w/cpp/language/types#char8_t)
- [Unicode 标准：字符、编码与 UTF-8 合法序列](https://www.unicode.org/versions/Unicode17.0.0/core-spec/chapter-3/)
- [P0482R6：char8_t 的类型与转换规则](https://wg21.link/p0482r6)
- cppreference: [std::u8string](https://zh.cppreference.com/w/cpp/string/basic_string)
