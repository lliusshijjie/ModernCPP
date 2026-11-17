# 第 49 课 · std::print / std::println（C++23）

> 所需标准：**C++23**（`-std=c++23`）｜ 独立代码：`49_print.cpp`
> 本机 MSYS2 GCC 15.1 的 print 终端支持还需链接 **`-lstdc++exp`**。
> 这是本机标准库的要求，不是 C++23 的通用链接选项；本课后面的示例也使用 print。
> 前置：第 36 课（std::format）

## 一、底层原理

### 1.1 格式化与输出合在一个接口中

std::format 返回字符串，print 把格式化结果写到输出目标；println 再追加换行。
它们沿用 format 的类型安全与编译期格式串检查。

| 接口 | 结果 | 状态与校验 |
|---|---|---|
| printf | 写到 C 流 | 说明符与参数类型不匹配可能 UB，编译器可额外诊断 |
| iostream | 写到 C++ 流 | 类型安全，格式操纵器可能修改流状态 |
| format | 返回 string | 类型安全、格式串检查，不直接输出 |
| print / println | 写到指定输出目标 | 沿用 format 规则，不依赖 cout 的格式状态 |

这不保证单次系统调用、无分配或一定比其他接口快。原始 C++23 的规范允许
先格式化到字符串再写；本机 GCC 15.1 也有相应缓冲，不能称为“零中间串直写”。

### 1.2 编码与终端感知

普通字面量编码为 UTF-8 时，print 选择 Unicode 输出路径；否则选择非 Unicode
路径。它不会自动识别并转码任意编码的 char 字符串，也不直接接受 u8string。
在需要原生 Unicode API 的终端上，标准库可进行相应转换；本机 Windows
终端路径使用原生支持。重定向到普通文件时通常直接写格式化后的代码单元。

因此不能一概要求先 chcp 65001，也不能认为设置代码页能修复错误编码的输入。
源/执行字符集、输入字符串编码、输出目标和标准库实现都需要一致。

### 1.3 换行、格式串与标准版本

- `std::println("hello")` 仍然传入了格式串，只是参数包为空，不是“无格式串直印”。
- 要打印含花括号的任意文本，用 `std::println("{}", text)`；把任意文本直接当
  格式串会触发格式解释。
- 原始 C++23 打印空行用 `std::println("")`。无参 `println()` 来自后续的
  **P3142R0**，本机库在 C++23 模式下作为扩展提供；本课示例采用原始接口写法。

## 二、具体语法

```cpp
#include <cstdio>
#include <print>

std::print("{} {}\n", "vec", 3);       // print 本身不追加换行
std::println("auto newline");
std::println("{:>8.2f}", 3.14159);
std::println("");                      // 原始 C++23 的空行写法
std::println(stderr, "warn: {}", 1);    // FILE* 目标
std::fflush(stdout);                   // 需要明确刷新时调用，并按需检查返回值
```

规则细则：

- 默认目标是 stdout；还可用 FILE* 重载。`<ostream>` 中另有接受 ostream&
  的 print/println 重载，不能说指定目标只有 FILE* 一条路。
- 动态格式串可用 vprint_unicode/vprint_nonunicode 配合 make_format_args；
  make_format_args 的参数及其引用对象须保持有效，编码路径应与输入约定匹配。
- println 没有 endl 那种“每次强制刷新”的接口承诺；但行缓冲遇到换行，或
  Windows 原生 Unicode 输出路径的流同步，仍可能导致实际刷新。
- printf 与 FILE* 版 print 使用同一个 C 流，不存在必然独立缓冲导致乱序的规则。
  iostream 默认与 C 标准流同步；关闭 sync_with_stdio 后混用须自行管理顺序。
  stdout/stderr 是不同流，不能依赖外部收集器按程序意图合并它们。
- 输出失败可能抛 system_error，格式化也可能抛异常；print 并非 noexcept。
- C++23 已有范围格式化（P2286）：支持它的标准库可直接打印 vector 等可格式化
  范围。本机支持；其他环境应核对 `__cpp_lib_format_ranges` 和元素格式化支持。

## 三、实际应用与最佳实践

### 3.1 场景：日志分流与报表

info 写 stdout，诊断写 stderr；格式宽度、精度等复用第 36 课。
需要明确的显示时机或提示符时显式刷新，处理写入失败。

### 3.2 场景：大量输出

测量格式化、缓冲、终端渲染和重定向文件各自的成本。需要批量写入时，可先用
format_to 构建缓冲，再写出；不要把接口名称中的 buffered 当成可由调用者
定期 flush 的日志队列。vprint_unicode_buffered 等来自后续 print 缺陷修正，
**本机 GCC 15.1 的头文件没有该接口**，不能直接作为本课可运行方案推荐。

### 3.3 最佳实践清单

1. 需要格式化输出时可优先选择 print，按项目需求保留其他 I/O 接口。
2. 普通文本通过 `"{}"` 传入，避免把外部文本当格式串。
3. 区分换行、流缓冲刷新与终端可见性，不假设 println 永远刷新或永远不刷新。
4. 测量性能，不从 API 名称推断分配次数或系统调用次数。
5. 链接选项按工具链配置；本机示例需 -lstdc++exp。

## 常见陷阱

1. 本机遗漏 -lstdc++exp 会缺少 Windows 终端支持符号。
2. `std::println("{}")` 没有对应参数，会因格式串不匹配而编译失败。
3. 动态 string 不能直接传给要求编译期格式串的 print；用类型擦除接口。
4. “println 只追加换行”不意味着实现永远不会刷新 C 流。
5. 范围格式化属于 C++23，不能把尚未实现的库支持误写成标准不支持。
6. 无参 println 与后续 buffered 接口需区分标准发布版本、修正和本机扩展。

## 完整示例

```bash
g++ -std=c++23 -Wall -Wextra "CPP23/49_std_print/49_print.cpp" -o run -lstdc++exp && ./run
```

## 练习

1. 把第 36 课的九九乘法表改成 println 版，保持原先格式。
2. 输出带回车的进度条并显式 fflush(stdout)，比较终端与重定向文件的行为。
3. 顺序调用 printf 和 print 写同一个 stdout，再比较关闭 C++ 流同步后的混用。
4. 为自定义类型提供 formatter，比较直接打印该对象及 vector<该类型>。

## 参考

- [C++23 草案：print 的编码选择、刷新与异常](https://timsong-cpp.github.io/cppwp/n4950/print.fun)
- [C++23 草案：ostream 版本](https://timsong-cpp.github.io/cppwp/n4950/ostream.formatted.print)
- [P3142R0：打印空行的后续补充](https://wg21.link/p3142r0)
- [P3107R5：print 实现效率的后续缺陷修正](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p3107r5.html)
- [P2539R4：原生终端路径与流同步](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2539r4.html)
