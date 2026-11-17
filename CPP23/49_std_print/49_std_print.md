# 第 49 课 · std::print / std::println（C++23）

> 所需标准：**C++23**（`-std=c++23`）｜ 讲解：本文件 ｜ 独立代码：`49_print.cpp`
> —— CPP23 篇开篇课
> ⚠ 本机工具链实测（g++ 15.1.0 / MSYS2）：编译用 `-std=c++23`，
> **链接需追加 `-lstdc++exp`**（Windows 终端支持的运行时符号被拆分在该库，
> 否则报 `undefined reference to std::__open_terminal`）——
> 编译命令见下文；前 5 课均需此旗子
> 前置：第 36 课（std::format——print 就是它的输出端）

## 一、底层原理

### 1.1 输出的三国志终于归一

| | printf | iostream | format（第 36 课） |
|---|---|---|---|
| 类型安全 | 无 | 有 | 有（编译期校验） |
| 写入方式 | 直接 | 大量状态/虚调用 | 先成串再输出 |
| 一步到位 | 是 | 是 | **否**（要再 cout 一次） |
| 换行便利 | 手写 `\n` | endl（还附带刷新） | 手写 |

`std::print / std::println`（P2093，源自 {fmt}）补上最后一格：
**类型安全 + 编译期格式校验 + 单次格式化直写 + println 自动换行**。
C++ 终于有了别的语言十年前就有的那句 `print("x = {}", x)`。

### 1.2 实现视角：format 的薄封装 + 终端感知

- `print(fmt, args...)` ≈ `vformat` 结果一次写出——
  无 iostream 的 locale/状态拖累，快路径接近 printf
- **UTF-8 为本**：格式串与文本参数按 UTF-8 对待；当 stdout
  **是终端**时实现可以做转码直写（Windows 上转宽字符走
  WriteConsoleW，规避代码页乱码）。本机 libstdc++ 的该部分
  支持就放在 `libstdc++exp`——这就是链接旗子的由来（如实标注）
- 编译期校验照搬第 36 课：`std::print("{:d}", "x")` 编译错

### 1.3 println 的两个省心细节

1. **自动换行**：`println("done")` 等价 `print("done\n")`
2. **无格式串直印**：没有参数要格式化时连 `{}` 都不用写——
   `println("hello")` 合法（C++23 起 print 家族的便捷重载）

## 二、具体语法

```cpp
#include <print>

std::print("{} {}\n", "vec", 3);      // 不换行版
std::println("auto newline");         // 自动换行
std::println("{:>8.2f}", 3.14159);    // 格式说明符全盘继承 format
std::println();                       // 空行
std::println(stderr, "warn: {}", 1);  // 指定流（FILE* 版）
std::print(stdout, "x");              // 同上

// 本机（MSYS2 g++ 15.1.0）编译命令——注意链接旗子：
// g++ -std=c++23 -Wall -Wextra 49_print.cpp -o run -lstdc++exp && ./run
```

规则细则：

- 格式串必须是**编译期常量**（同 format，P2216）；动态串走
  `std::vprint_unicode / vprint_unicode_buffered`
- 默认目标是 `stdout`；要 stderr 必须显式传 `FILE*`
- `print` **不主动刷新**（stdout 行缓冲/全缓冲照旧）；
  要即时可见传 `stderr` 或手动 `fflush`
- 与 iostream 混用注意**交错**：二者缓冲独立，
  关键输出统一走一个体系
- 宽字符版 `std::wprint` 不存在（C++26 候选）—— wchar 输出仍归 wcout

## 三、实际应用与最佳实践

### 3.1 场景：日志三件套（示例 ③）

info 走 stdout、warn/error 走 stderr、格式对齐复用第 36 课的
说明符——一套 print 全包，再无 `printf("%s", ...)` 与
`cout << left << setw` 的混搭。

### 3.2 场景：循环输出提速（示例 ②）

十万行日志：`print` 单次格式化直写，比 `cout << ... << '\n'`
少一层流状态；若吞吐敏感再加 `vprint_unicode_buffered` 手动
批量刷新。

### 3.3 最佳实践清单

1. 新代码输出一律 `println`；printf 冻结、iostream 降级为
   无格式二进制/宽字符场景
2. 错误/诊断走 `println(stderr, ...)`——重定向语义天然正确
3. 非 ASCII 文本在 Windows 控制台需 `chcp 65001`；程序内
   保证 UTF-8 源（本课程示例坚持 ASCII 输出同理）
4. 性能敏感的大批量输出考虑 buffered 变体 + 定期 flush
5. 别用 `endl`（早已该淘汰）——`println` 连"要不要刷新"都替你想好了

## 常见陷阱

1. **本机忘记 `-lstdc++exp`**：编译过、链接挂（undefined
   reference to `std::__open_terminal`）——不是代码错，是库没链
2. `print` 写了却"看不到"：stdout 全缓冲 + 没 flush——
   管道/文件重定向时尤其明显；调试输出用 stderr 或显式 flush
3. 格式串不是常量（运行期拼接）→ 编译错——动态串走
   `vprint_unicode`（同第 36 课 vformat 的关系）
4. 以为 println 会 flush：**不会**（它只追加 '\n'）——
   endl 的"换行即刷新"语义没有被继承，这是好事也是坑
5. 容器仍然不能直接 `{}` 打印（那是 C++23 的 format 范围化，
   P2286，编译器支持尚不齐）——见第 59 课
6. `std::print` 与 `printf` 输出交错：缓冲体系不同——
   混用输出会乱序，统一到 print 家族

## 完整示例

见 `49_print.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++23 -Wall -Wextra "CPP23/49_std_print/49_print.cpp" -o run -lstdc++exp && ./run
```

## 练习

1. 把第 36 课练习 1 的九九乘法表改成 println 版，对比代码长度。
2. 写"进度条"：同一行反复 `print("\r{:-<20}| {}%", ...)`，
   观察 `\r` 覆盖刷新效果（终端需支持，重定向到文件则原样堆积）。
3. 验证缓冲：向文件重定向跑 `./run > out.txt`，对比 print 与
   stderr 输出的出现时机（配合 `fflush(stdout)` 实验）。
4. 查阅并试用自己的 `std::formatter` 特化（第 59 课预备），
   让一个自定义类型能被 println 直接打印。

## 参考

- 提案 P2093R14（print/println）/ P2589R1（无格式串直印）
- cppreference: [std::print](https://zh.cppreference.com/w/cpp/io/print)
- cppreference: [std::println](https://zh.cppreference.com/w/cpp/io/println)
