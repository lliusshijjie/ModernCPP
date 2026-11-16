# 第 40 课 · consteval、constinit 与 std::source_location（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`40_consteval_constinit_source.cpp`
> 前置：第 02 课（constexpr 增强）；第 26 课（inline 变量与静态初始化顺序）

## 一、底层原理

### 1.1 三兄弟各管一段"时间"

| 关键字 | 用在 | 编译期 | 运行期 | 一句话 |
|---|---|---|---|---|
| `constexpr` | 函数/变量 | 可算 | **也允许调用** | "能"在编译期 |
| `consteval` | 函数 | **必须**在此 | 调用即编译错 | "只能"在编译期 |
| `constinit` | 变量 | **必须**在此**初始化** | **可自由修改** | 初始化时序上保险 |

`constexpr` 函数是"双模"的：传常量走编译期、传变量走运行期——
好处是灵活，代价是**没人保证它真的在编译期跑**。`consteval`
（立即函数，immediate function）堵死运行期通道：**每一次调用都必须
产出常量表达式**，否则编译错。编译表、安全断言、单位换算这类
"绝不该等到运行时"的逻辑从此有了硬约束。

### 1.2 constinit：治的是"静态初始化顺序灾难"

全局变量 `int x = f();`（f 非 constexpr）是**动态初始化**：跑到
main 之前由启动代码执行，跨翻译单元的初始化顺序**未定义**——
第 26 课提过的静态顺序灾难（static initialization order fiasco）。
`constinit` 强制**常量初始化**：初始化器必须是常量表达式，值在编译期
进镜像，启动代码无事可做——顺序问题从根上消失。它与 `const` 无关：
`constinit int counter = 7;` 之后 `++counter` 完全合法。

### 1.3 source_location：把 `__FILE__`/`__LINE__` 扶正为类型

`std::source_location::current()` 返回含 `file_name/line/column/
function_name` 的对象，且是 `constexpr` 友好的正规类型。真正杀招在
**默认实参**处调用：默认实参在**调用方**求值——于是 `log(msg, loc =
source_location::current())` 拿到的是**调用点的位置**，宏
`__FILE__ __LINE__` 的两大宿命（只能用宏、污染所有调用形态）一起终结。

## 二、具体语法

```cpp
consteval int sq(int x) { return x * x; }   // 立即函数
static_assert(sq(3) == 9);                  // 编译期使用
int a[sq(2)];                               // 立即调用可作数组长度
int r = sq(2) * 10 + runtime_val;           // 嵌在运行期表达式里：
                                            // sq(2) 这一段仍是编译期求值
// int bad = sq(runtime_val);               // 编译错：实参非常量

constinit int counter = 7;                  // 常量初始化的全局可变量
++counter;                                  // 运行期修改，合法
// constinit int bad = runtime_fn();        // 编译错：须常量表达式
// 函数体内的 constinit 声明                // 编译错：仅限静态/线程存储期

void log(std::string_view msg,
         std::source_location loc = std::source_location::current());
log("hi");   // loc 是"调用方"的位置（默认实参在调用点求值）
auto here = std::source_location::current();   // 函数体内取 = 本函数位置
```

规则细则：

- 立即函数可以互相调用；`constexpr` 函数体内调用 `consteval` 后，
  该 `constexpr` 函数实际也只能在编译期实例化成功
- `consteval` 不接受函数指针取址后运行期调用式的绕路（地址只能进
  常量表达式上下文）
- `source_location` 四成员：`file_name()`/`line()`/`column()`/
  `function_name()`；`function_name()` 的内容实现定义（GCC 带完整签名，
  如 `int main()`）
- `source_location::current()` 本身可在编译期调用，能进
  `static_assert` 消息以外的常量语境
- 三者组合常见形态：`consteval` 造表 → `constexpr` 数组接住 →
  运行期只读使用（示例 ③）

## 三、实际应用与最佳实践

### 3.1 场景：编译期表与校验（示例 ①/③）

CRC 表、单位换算表、正弦查表——`consteval` 生成 + `constexpr` 接住，
运行期零启动开销；参数合法性在 `consteval` 里 `throw`（编译期即报错）
比运行期断言早一整层。

### 3.2 场景：无宏日志/错误上报（示例 ②/③）

`log(msg, loc = current())` 默认实参拿调用方位置，函数体内的
`current()` 拿自己的位置——两层语义各取所需。日志、assert、
配置错误上报不再需要任何宏。

### 3.3 最佳实践清单

1. "只该编译期发生"的逻辑（造表、常量校验）一律 `consteval` 锁死；
   通用工具函数保持 `constexpr` 双模
2. 全局/静态**可变**状态声明 `constinit`——初始化时序交给编译器背书
   （只读的直接 `constexpr`）
3. 位置信息默认实参化：接口写 `loc = std::source_location::current()`，
   调用方零负担
4. 立即函数里不要写依赖运行环境的分支（locale、随机、时钟）——
   编译器会立刻教你做人
5. 发布产物想要"查表值进只读段"：`constexpr` 变量接住 `consteval` 结果，
   别让造表函数在运行期再跑一遍

## 常见陷阱

1. **以为 consteval 函数"运行得更早"**：不，它是"只能编译期"，
   不是"自动提前"——没有常量语境的调用直接编译错
2. **constinit 当 const 用**：它不锁写只锁初始化方式；要只读加 `const`
   或改 `constexpr`
3. **局部变量写 constinit**：编译错——它只作用于静态/线程存储期
4. `constexpr` 函数悄悄退化为运行期执行（传了运行期实参）却不自知——
   需要硬保证时用 `consteval` 或 `static_assert` 钉住
5. `source_location` 默认实参写在了**函数体内**：拿到的是被调函数自己的
   位置而不是调用方——两处语义相反，位置差一行结果差千里
6. GCC 的 `function_name()` 带签名（`int main()`），MSVC 可能只有名——
   解析/展示代码别假设固定格式

## 完整示例

见 `40_consteval_constinit_source.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++20 -Wall -Wextra "CPP20/40_consteval_constinit与source_location/40_consteval_constinit_source.cpp" -o run && ./run
```

## 练习

1. 写 `consteval int clamp_index(int i, int n)`，越界时编译期报错
   （提示：`throw` 在常量求值中变成编译错）；用 `static_assert` 验证。
2. 把一段运行期查表代码（循环填充数组）改成 `consteval` 造表 +
   `constexpr` 接住，对比可执行文件大小与启动逻辑。
3. 复现陷阱 5：默认实参版与函数体内 `current()` 各打印一行，
   对比 line/function_name 差异。
4. 给全局 `constinit` 计数器写一个"递增 + 读数"的多函数程序，
   并解释为什么它没有静态初始化顺序问题（对照第 26 课）。

## 参考

- 提案 P1073R3（consteval 立即函数）/ P1143R2（constinit）/
  P1208R6（std::source_location）
- cppreference: [consteval](https://zh.cppreference.com/w/cpp/language/consteval)
- cppreference: [constinit](https://zh.cppreference.com/w/cpp/language/constinit)
- cppreference: [std::source_location](https://zh.cppreference.com/w/cpp/utility/source_location)
