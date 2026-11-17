# 第 47 课 · Modules 模块入门（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 接口单元：`47_weather_module.cpp`
> ｜ 导入方：`47_modules_main.cpp`
> 本机 GCC 15.1 还需显式启用模块。下文使用该版本文档规定的
> **`-fmodules-ts`**；本机也接受 `-fmodules`。这些选项不是 C++ 标准的一部分。

## 一、底层原理

### 1.1 从文本包含到模块接口

`#include` 在预处理阶段包含头文件文本。头文件保护避免同一翻译单元内
重复定义，却不能省掉每个翻译单元各自解析头文件的工作。模块让编译器
保存可供导入的接口信息，减少重复解析并控制名字的导出。

| | 头文件 | 命名模块 |
|---|---|---|
| 传播方式 | 预处理后的文本 | 编译器生成的接口信息（BMI；GCC 称 CMI） |
| 重复解析 | 不同翻译单元通常重复解析 | 可复用接口，仍有导入与依赖处理成本 |
| 宏 | 文本包含可传播宏 | 不传播命名模块的宏；头文件单元规则不同 |
| 名字可见性 | 声明、作用域与访问控制 | export 控制向导入方暴露的接口名字 |
| ODR | 必须遵守 | 仍须遵守，并不自动消除错误声明或重复定义 |

构建收益取决于依赖图、编译器、缓存和构建系统，应实际测量。
BMI 通常不兼容其他编译器，甚至不兼容同一编译器的不同版本/选项。

### 1.2 接口、实现与分区

```cpp
module;                    // 全局模块片段开始，便于包含传统头
#include <string>
export module weather;     // 主接口单元
int hidden_helper(int);    // 未导出，导入方不能直接按此名字查找
export namespace weather {
struct Reading { /* ... */ };
std::string describe(const Reading&);
}
```

- `export module weather;`：主接口单元，每个命名模块恰好一个。
- `module weather;`：不带分区名的实现单元，隐式导入主接口。
- `export module weather:stats;`：接口分区，主接口用 `export import :stats;`
  重导出；`module weather:impl;` 则是实现分区。
- `import weather;`：按模块名导入，并非按文件路径包含。

**点号不是分区符号**：`weather.stats` 是独立模块名；`weather:stats`
才是 weather 的分区。`import :stats;` 只能写在 weather 自己的模块单元中，
普通 main.cpp 不能这样直接导入其他模块的分区。

### 1.3 export 是名字可见性控制

本例导入方直接调用 hidden_helper(1) 会失败，因为该名字未导出。
但“未 export”不等于所有实体都像类的 private 成员一样无法使用：
导出接口的定义可能依赖未导出的类型/函数，其声明可达性、模板实例化
和 ADL 另有规则。模块不是代码保密或自动满足 ODR 的机制。

命名模块的宏不随 import 传播。**头文件单元**（如 `import <vector>;`）
与命名模块不同，导入方可能获得其宏，不能把两者一概而论。

### 1.4 工具链与标准版本

- GCC 15.1 可编译本课示例，但支持仍不完整，包括 private module fragment
  和部分分区可见性规则；不能称为完整支持。
- 接口编译生成对象文件和 CMI，默认缓存为 gcm.cache；导入前须构建接口。
- 传统标准头可放在全局模块片段中包含。GCC 头文件单元还需预先构建，
  不能仅把 #include 改成 import 就假定可用。
- **`import std;` 属于 C++23 的标准库模块支持**，不是 C++20 的保证；
  是否可用还取决于标准库和构建配置，本课不使用它。
- Clang/MSVC 的选项和产物不同；工程中应使用支持模块依赖扫描的构建系统，
  核对相应编译器和生成器的支持范围。

## 二、具体语法

```cpp
// ---- 模块接口 ----
module;
#include <string>          // 传统头通常放在全局模块片段
export module weather;
export int api();
int impl_detail();         // 非导出声明

// ---- 普通导入方（另一个翻译单元）----
#include <iostream>
import weather;
int main() { /* 使用导出接口 */ }
```

规则细则：

- import 必须位于全局命名空间作用域，不能放进函数或命名空间体。
  **模块单元**中的 import 必须在其相应声明序列的其他声明之前；
  没有 module 声明的普通翻译单元并无“必须在所有声明之前”的同一限制。
- 模块声明后出现 #include 并非语法上一律禁止；其中声明可能被附着到
  命名模块，造成冲突或可达性问题。传统第三方/标准头通常应放在全局模块
  片段，或使用正确构建的头文件单元。
- module/import 声明各自写在独立行，避免模块预处理识别与工具链解析问题。
- 模块名可含点号（如 net.http），与文件路径无强制对应关系。
- A 仅 import B 不自动把 B 的导出名字暴露给 A 的导入方；
  需要转发时在接口中用 **`export import B;`**。
- 模块内实体仍受 ODR、链接和可达性规则约束，不能任意重复定义。

## 三、实际应用与最佳实践

### 3.1 场景：明确库的发布面

用 export 列出接口，把可独立编译的非模板实现移到实现单元；模板和内联
函数等定义仍可能需要保留在可达接口中。本例导出 Reading、describe、
adjusted，不导出辅助函数与宏。

### 3.2 场景：减少下游重复构建工作

稳定接口与易变实现分开后，**只修改实现单元**通常能减少导入方重编。
接口单元改动、CMI 再生成、构建规则和工具链行为仍可能触发下游重建，
不能承诺“接口看起来未变就一定不重编”。

### 3.3 最佳实践清单

1. 先用小模块验证工具链、依赖扫描、增量构建和调试流程。
2. 用全局模块片段接入传统头，避免误把外部声明附着到命名模块。
3. 共享配置值可导出 constexpr 常量；需要预处理条件的宏不能用变量直接替代。
4. 按依赖图先构建被导入接口，再构建依赖它的单元；实现单元与普通导入方
   之间若无额外依赖，并不要求串行构建。
5. 保留源码与构建配置，不承诺 BMI 可跨任意工具链复用。

## 常见陷阱

1. 只写 -std=c++20：本机仍需显式模块选项，见下文。
2. 把 weather.stats 当作分区：分区须写 weather:stats。
3. 把命名模块的宏隔离规则套到头文件单元：两者不同。
4. 先编导入方：找不到 CMI 时无法导入；链接时还须提供所需对象文件。
5. import 放进函数不合法；放在普通翻译单元全局作用域中部则不必然非法。
6. 不兼容工具链共享 gcm.cache：应各用匹配的缓存。

## 完整示例

本课使用两个翻译单元，在本课目录先构建接口，再构建导入方：

```bash
cd "CPP20/47_Modules入门"
g++ -std=c++20 -fmodules-ts -Wall -Wextra -c 47_weather_module.cpp -o 47_weather_module.o
g++ -std=c++20 -fmodules-ts -Wall -Wextra 47_modules_main.cpp 47_weather_module.o -o run && ./run
```

GCC 在当前构建目录生成 gcm.cache。实验也可在独立构建目录中进行，使用
源码的绝对路径，以免不同实验的接口缓存互相干扰。

## 练习

1. 导出 hidden_helper，重新构建接口与导入方，再验证原先的非法调用。
2. 新建 export module weather:stats 接口分区，导出 average；主接口中
   export import :stats。先构建分区，再构建主接口和导入方。
3. 验证命名模块的宏不传播，再实验导出 constexpr 常量的值访问。
4. 比较头文件版与模块版同一小库的全量和增量构建耗时，计入接口构建成本。

## 参考

- [C++20 草案：模块单元与分区](https://timsong-cpp.github.io/cppwp/n4861/module.unit)
- [C++20 草案：import 的作用域与顺序](https://timsong-cpp.github.io/cppwp/n4861/module.import)
- [C++20 草案：export 与可见性](https://timsong-cpp.github.io/cppwp/n4861/module.interface)
- [GCC 15.1 模块支持与限制](https://gcc.gnu.org/onlinedocs/gcc-15.1.0/gcc/C_002b_002b-Modules.html)
- 提案 P1103R3（Merging Modules）
