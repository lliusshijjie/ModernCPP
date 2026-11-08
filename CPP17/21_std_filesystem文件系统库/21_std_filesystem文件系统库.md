# 第 21 课 · std::filesystem 文件系统库（C++17）

> 所需标准：**C++17** ｜ 讲解：本文件 ｜ 独立代码：`21_filesystem.cpp`

## 一、底层原理

### 1.1 三类角色：path / directory_entry / 迭代器

`<filesystem>`（源自 boost::filesystem）把文件系统操作拆成三层：

| 类型 | 职责 | 接触磁盘吗 |
|---|---|---|
| `fs::path` | 路径的**词法**表示：分解、拼接、规范化 | **不**——纯字符串操作 |
| `fs::directory_entry` | 路径与可缓存的属性 | 构造、刷新或查询时可能访问；缓存细节依实现而定 |
| `directory_iterator` / `recursive_directory_iterator` | 遍历目录 | 遍历时查 |

理解这个库的钥匙：**分清"词法操作"与"查盘操作"**。
`p.filename()`、`lexically_normal()` 永远只动字符串（不存在的路径也能算）；
`fs::exists(p)`、`fs::file_size(p)` 才真的访问文件系统。

### 1.2 path 的两副面孔

Windows 原生分隔符是 `\`，POSIX 是 `/`。path 内部记住来源格式，
`native()` 按平台输出、`generic_string()` 永远输出 `/`。
**跨平台代码输出/存储路径一律 `generic_string()`**——本课示例全程如此。

### 1.3 错误处理双轨制：标准库最系统的 error_code 实践

许多文件系统操作提供异常版与错误码版重载，例如：

| 版本 | 签名特征 | 错误去向 |
|---|---|---|
| 抛异常版 | `file_size(p)` | 抛 `fs::filesystem_error`（含 path1/path2） |
| 错误码版 | `file_size(p, ec)`（noexcept） | 填 `std::error_code`，不抛 |

错误属于异常情况时可以选异常版，缺失文件等属于常规分支时可以选错误码版。
错误码重载不代表整个操作链都不抛异常：应查看具体函数的异常约定，
并单独考虑路径构造、内存分配等操作。每次调用之后都应及时检查 ec。

## 二、具体语法

### 2.1 path：构造、拼接、分解

```cpp
fs::path p = "dir/hello.txt";
p.parent_path();  p.filename();  p.stem();  p.extension();   // dir / hello.txt / hello / .txt
fs::path q = fs::path("dir") / "sub" / "f.txt";              // 拼接自动补分隔符
p /= "more";                                                 // 追加
p.lexically_normal();        // "a/b/../c" → "a/c"（纯词法，不查盘）
p.is_absolute();
```

注意：`/` 是分隔符拼接；想**无分隔符**连接用 `+=`（或 `concat`）——
`path` 压根没有 `operator+`，写 `p + "x"` 直接编译错（编译器替你把关）。

### 2.2 查盘与修改

```cpp
fs::exists(p);  fs::is_directory(p);  fs::is_regular_file(p);
fs::file_size(p);                       // 普通文件返回字节数，其他类型结果由实现定义
fs::create_directories("a/b/c");        // 递归建目录（mkdir -p）
fs::copy(src, dst);  fs::rename(a, b);
fs::remove(p);      fs::remove_all(p);  // 分别返回 bool 与删除条目数
fs::current_path();
```

### 2.3 遍历

```cpp
for (const auto& e : fs::directory_iterator(dir)) { e.path(); e.is_directory(); }
for (const auto& e : fs::recursive_directory_iterator(dir)) {...}  // 递归下降
```

**遍历顺序未指定**——需要确定顺序就先收集进 vector 再 sort（示例 ③ 的做法）。

## 三、实际应用与最佳实践

### 3.1 场景：统计一个目录树（示例 ③-1）

递归遍历 + 过滤 `.cpp` + 汇总大小——构建脚本/代码统计工具的核心循环，
三十行内解决。

### 3.2 场景：健壮的批处理（示例 ③-2）

`create_directories` 幂等（已存在不报错）、`remove_all` 清场、
错误码版查属性——临时工作目录的标准生命周期。

### 3.3 最佳实践清单

1. 跨平台输出/持久化路径用 `generic_string()`，别裸 `<<` path
2. 拼路径一律 `operator/`，禁止手写 `"/" + name`
3. 遍历顺序不保证——先收集再排序，测试才可复现
4. 遍历时可优先使用 `directory_entry` 的属性查询，利用实现提供的缓存；
   缓存范围和系统调用次数不能一概而论，缓存也不保证始终最新
5. 错误处理按场景选轨：交互用异常、批量用 error_code；混用前想清楚
6. 遍历中增删目录条目是自找麻烦——先收集名单，遍历完再改

## 常见陷阱

1. `path` 没有 `operator+`（编译错，好事）；无分隔符连接要显式 `+=`——
   但 99% 的场景你要的是 `/`
2. `is_regular_file` 对目录通常返回 false；`file_size` 对非普通文件结果由实现定义，
   需要统计普通文件时先判断类型，再检查大小查询的错误
3. Windows 下 `native()` 输出反斜杠，直接拼进跨平台协议就炸——统一 generic
4. `remove_all` 的返回值包含**目录本身**（文件 + 目录都计数）——
   断言具体数字前先跑一遍确认
5. 递归遍历默认**不跟随符号链接**（防环）——想跟随要传
   `fs::directory_options::follow_directory_symlink`

## 完整示例

见 `21_filesystem.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织
（③ 自建临时目录、写文件、统计、清场，全程确定输出）：

```bash
g++ -std=c++17 -Wall -Wextra "CPP17/21_std_filesystem文件系统库/21_filesystem.cpp" -o run && ./run
```

## 练习

1. 扩展示例统计：按扩展名分组计数（`.cpp/.txt/其他`），
   输出每组的文件数与总字节数。
2. 写 `find_newest(dir)`：返回 `directory_entry`（比较 last_write_time），
   注意处理空目录的 `optional` 返回。
3. 把 ③ 的遍历改成错误码版（`fs::directory_iterator(dir, ec)`），
   对一个不存在的目录验证 `ec` 被置位而不抛。
4. 用 `lexically_normal` 清洗用户输入的 `"./a/../b//c/./d.txt"`，
   再分离出目录链上的每一级（循环 `parent_path`）。

## 参考

- 提案 P0218 / C++17 [fs] 章（源出 boost::filesystem，2003 年实践）
- cppreference: [std::filesystem](https://zh.cppreference.com/w/cpp/filesystem)

## 补充阅读

[21A · 路径规范化与分层错误处理](../补充知识点/21_补充A_路径规范化与分层错误处理/21_补充A_路径规范化与分层错误处理.md)：区分词法路径与实际文件身份，逐层检查查询和迭代错误。
