# 第 47 课 · Modules 模块入门（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：两个 cpp
> —— 接口单元：`47_weather_module.cpp` ｜ 导入方：`47_modules_main.cpp`
> （模块无法单文件演示，参照补充课 26A 的多文件先例）
> ⚠ 本机工具链实测：g++ 15.1.0 需要显式加 **`-fmodules`** 才启用
> module/import 关键字（只给 `-std=c++20` 会报 'module' does not name
> a type）；编译命令见下文

## 一、底层原理

### 1.1 头文件的原理性缺陷

`#include` 是**文本粘贴**：一个头被包含 N 次就解析 N 遍（编译时间）、
宏与声明互相污染（include 顺序玄学）、同名实体在不同 TU 的 ODR
全靠人肉守恒（第 26 课）。三十年来所有"正确用法"都是在给文本粘贴
打补丁。模块换掉的是**机制本身**：

| | 头文件 | 模块 |
|---|---|---|
| 传播方式 | 文本包含 | 编译好的**接口描述**（BMI/CMI 二进制） |
| 重复解析 | 每个 TU 一次 | **一次**，之后直接读摘要 |
| 宏 | 全局泄漏 | **不跨模块**（每 TU 独立） |
| 可见性控制 | 全有或全无（pimpl 除外） | **export 精确导出**，其余私有 |
| ODR 风险 | 人肉保证 | 模块实体天然全局唯一 |

### 1.2 三种翻译单元与接口单元的解剖

```cpp
module;                    // ① 全局模块片段：传统 #include 只许放这里
#include <string>

export module weather;     // ② 模块声明：本单元提供模块 weather 的接口

int hidden_helper(int);    // ③ 未 export —— 模块私有

export namespace weather { // ④ export 的实体才对导入方可见
struct Reading { ... };
std::string describe(const Reading&);
}
```

配套形态：`module weather;`（**实现单元**：同模块的私有实现）、
`import weather;`（**导入方**）、`export module weather.partition;`
（**分区**：把大模块拆文件，本课不展开）。

### 1.3 隔离墙：export 之外皆私有

头文件世界里"包含了就能用"；模块世界里**没 export 的名字对导入方
不存在**——`hidden_helper` 在接口单元里定义，导入方调用它直接编译错。
宏同理不跨模块。这是 C++ 第一次有了**真·封装边界**（对比：匿名
namespace 只防名字冲突，不防可见性）。

### 1.4 工具链现状（如实标注）

- 本机 g++ 15.1.0：命名模块完整可用，但**须显式 `-fmodules`**
  （GCC 的历史包袱：旗子先行、尚未并入 `-std=c++20` 默认）
- BMI（GCC 叫 CMI，`gcm.cache/` 目录）由接口单元编译时自动生成，
  导入方编译时自动读取——**接口先编、导入后编**的顺序约束由此而来
- `import std;`（标准库整体成模块）：**本机不可用**（libstdc++ 15
  尚未随发行预构建）——标准头仍走 `#include`（全局模块片段）
- clang/msvc 旗子与产物各不相同——构建系统（CMake 3.28+）是
  工程化的正道

## 二、具体语法

```cpp
// ---- 接口单元 weather_mod.cpp ----
module;                    // 全局模块片段开头（要 #include 传统头就先写它）
#include <string>
export module weather;     // 模块声明（每个模块一个接口单元）
export int api();          // 单个导出
export namespace w { ... } // 整块导出
int impl_detail();         // 不写 export：私有

// ---- 导入方 main.cpp ----
import weather;            // 一条导入语句（不用头文件路径）
import :part;              // 导入分区（本课不展开）

// ---- 编译（本机 g++ 15.1.0 实测命令）----
// g++ -std=c++20 -fmodules -c 47_weather_module.cpp -o 47_weather_module.o
// g++ -std=c++20 -fmodules 47_modules_main.cpp 47_weather_module.o -o run
```

规则细则：

- `import` 必须出现在文件**顶部**（所有本模块声明之前）
- 头文件包含不能出现在模块声明**之后**——要包含就进全局模块片段
- 模块名可用点分层（`net.http.router`），与文件路径无强制对应
- 同一模块的实体在所有 TU 中是**同一实体**（ODR 天然满足）
- 头文件单元（`import <vector>;`）与命名模块并存——迁移期混搭合法
- 导入的名字**不传递**（A 导入 B，不代表导入 A 能看见 B 的导出）

## 三、实际应用与最佳实践

### 3.1 场景：库的发布面（示例双文件）

`export` 列表就是**机器可读的 API 文档**——
公私边界不再靠"放 private 目录 + 注释约定"。
示例中 `adjusted` 公开、`hidden_helper` 私有，导入方一试便知（示例
注释里的两条编译错）。

### 3.2 场景：编译提速

大头文件（Eigen/Boost 级）改模块接口：下游 N 个 TU 从"N 遍解析"
变"读 1 份摘要"——大型项目分钟级收益常见。增量构建时接口不变
则下游不重编。

### 3.3 最佳实践清单

1. 新起的独立库优先试模块（先小后大）；存量代码混搭迁移
   （头文件单元过渡）
2. 接口单元只放**导出与必要声明**，实现挪到实现单元——
   接口改实现不改时，下游免重编
3. 宏想跨模块传播：export 一个 `constexpr` 常量（第 26 课的
   现代化思路在模块时代更彻底）
4. 构建顺序：接口 → 实现单元 → 导入方；交给构建系统管，
   别手敲
5. 教学与本地验证记住两个词：`-fmodules`、先编接口

## 常见陷阱

1. **忘了 `-fmodules`**（本机）：报 "'module' does not name a type"——
   错因在旗子不在代码；本课开头即如实标注
2. **接口后写 #include**：模块声明之后不能再包含传统头——
   要包含进全局模块片段（`module;` 之后、`export module` 之前）
3. 以为 import 会带出宏：**不会**。需要"宏式常量"就导出 constexpr
4. **编译顺序颠倒**：先编导入方会找不到 BMI（gcm.cache 里没有）——
   报 "imports must be built before being imported"
5. `import` 写在函数/文件中部：非法——必须在模块声明区
6. 跨编译器直接搬 gcm.cache：BMI **不通用**（编译器甚至版本专属），
   清理后各编各的

## 完整示例

本课为双文件示例（先编接口，再编导入方）：

```bash
cd "CPP20/47_Modules入门"
g++ -std=c++20 -fmodules -Wall -Wextra -c 47_weather_module.cpp -o 47_weather_module.o
g++ -std=c++20 -fmodules -Wall -Wextra 47_modules_main.cpp 47_weather_module.o -o run && ./run
```

## 练习

1. 把接口里的 `hidden_helper` 加上 `export`，重新编译两步——
   观察 47_modules_main.cpp 原本注释掉的非法调用是否变为合法。
2. 新增模块分区：`export module weather.stats;` 提供
   `average(reading 列表)`，主模块 `export import :stats;` 转发——
   验证导入方无感。
3. 在接口单元定义宏并在导入方使用（编译错）；改成导出 constexpr
   常量后成功——记录两种形态的报错/通过差异。
4. 对比实验：同一个 5 函数小库写成头文件版与模块版，各编译
   两个 TU，对比 `time g++` 的总耗时（模块版记得先编接口）。

## 参考

- 提案 P1103R3（模块工具与交互）/ N4720（模块核心语言规范）
- GCC 文档: [C++20 Modules](https://gcc.gnu.org/onlinedocs/gcc/C_002b_002b20-Modules.html)
- cppreference: [模块（modules）](https://zh.cppreference.com/w/cpp/language/modules)
