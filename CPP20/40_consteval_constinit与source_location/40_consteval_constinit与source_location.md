# 第 40 课 · consteval、constinit 与 std::source_location（C++20）

> 所需标准：**C++20**（`-std=c++20`）｜ 讲解：本文件 ｜ 独立代码：`40_consteval_constinit_source.cpp`
> 前置：第 02 课（constexpr 增强）；第 26 课（inline 变量与静态初始化顺序）

## 一、底层原理

### 1.1 三兄弟各管一段"时间"

| 关键字 | 用在 | 编译期 | 运行期 | 一句话 |
|---|---|---|---|---|
| `constexpr` | 函数/变量 | 函数可参与常量求值；变量须满足常量初始化要求 | 函数也可运行期调用；变量为 const | 常量表达式能力/常量变量 |
| `consteval` | 函数 | 立即调用必须是常量表达式 | 可嵌入运行期表达式，立即调用部分仍常量求值 | 立即函数 |
| `constinit` | 静态/线程存储期变量 | 保证静态初始化，不允许动态初始化 | 可修改（除非另有 const） | 约束初始化方式 |

`constexpr` 函数是"双模"的：在要求常量表达式的上下文中须常量求值；
普通调用即使传常量也不保证标准意义上的常量求值，编译器仍可能优化折叠——
好处是灵活，代价是**没人保证它真的在编译期跑**。`consteval`
（立即函数，immediate function）要求：在立即函数上下文之外的
**立即调用必须是常量表达式**，否则编译错。立即函数内部调用其他立即函数时，
允许先传入形参，再在最外层立即调用时统一完成常量求值。编译表、安全断言、单位换算这类
"绝不该等到运行时"的逻辑从此有了硬约束。

### 1.2 constinit：治的是"静态初始化顺序灾难"

全局变量 `int x = f();`（f 非 constexpr）通常需要**动态初始化**：
通常由 main 之前的启动代码执行，也可按标准规则延迟；跨翻译单元的顺序取决于
初始化类别及依赖关系，不能依赖任意两个普通全局对象的先后次序，
也不能把顺序不确定直接称为"未定义行为"。
`constinit` 禁止动态初始化，保证**静态初始化**（零初始化或常量初始化）。
它能避免这个变量自身的动态初始化顺序问题，但不解决其他对象的初始化、
析构次序或运行期并发访问。它不隐含 `const`：
`constinit int counter = 7;` 之后 `++counter` 完全合法。

### 1.3 source_location：把 `__FILE__`/`__LINE__` 扶正为类型

`std::source_location::current()` 返回含 `file_name/line/column/
function_name` 的对象，且是 `constexpr` 友好的正规类型。真正杀招在
**默认实参**处调用：默认实参在**调用方**求值——于是 `log(msg, loc =
source_location::current())` 拿到的是**调用点的位置**，无需宏也能记录调用位置。这是 `current()` 在默认实参中的专门语义；
若改成默认实参调用一个 helper，再在 helper 内调用 current()，
得到的会是 helper 内部的位置。

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
void work() {
    static constinit int local = 1;           // 合法：局部静态变量
    ++local;
    // constinit int bad_local = 1;           // 错：自动存储期
}

void log(std::string_view msg,
         std::source_location loc = std::source_location::current());
log("hi");   // loc 是"调用方"的位置（默认实参在调用点求值）
auto here = std::source_location::current();   // 函数体内取 = 本函数位置
```

规则细则：

- 立即函数之间可以调用。普通 `constexpr int f(int x) { return sq(x); }`
  在 C++20 中不合法：x 不是此处立即调用所需的常量表达式；
  `constexpr int f(int x) { return sq(2) + x; }` 则仍可在运行期调用。
  不要与后续缺陷修正对某些模板/lambda 的立即函数传播规则混淆
- 立即函数的地址可在立即函数上下文中临时取得并使用，但不能作为
  常量表达式的最终结果逃逸给运行期；`auto p = &sq;` 不合法
- `source_location` 四成员：`file_name()`/`line()`/`column()`/
  `function_name()`；`function_name()` 的内容实现定义（GCC 带完整签名，
  如 `int main()`）
- `source_location::current()` 本身可在编译期调用，能进
  `static_assert` 消息以外的常量语境
- 三者组合常见形态：`consteval` 造表 → `constexpr` 数组接住 →
  运行期只读使用（示例 ③）

## 三、实际应用与最佳实践

### 3.1 场景：编译期表与校验（示例 ①/③）

CRC 表、单位换算表、正弦查表——用支持常量求值的算法，
`consteval` 生成 + `constexpr` 接住（C++20 的 `std::sin` 不能直接用于常量求值），
运行期零启动开销；参数合法性在 `consteval` 里 `throw`（编译期即报错）
比运行期断言早一整层。

### 3.2 场景：无宏日志/错误上报（示例 ②/③）

`log(msg, loc = current())` 默认实参拿调用方位置，函数体内的
`current()` 拿自己的位置——两层语义各取所需。日志、assert、
配置错误上报可用普通函数获取调用位置；若还需要字符串化表达式、
条件编译或像 assert 那样在发布版移除表达式求值，仍可能需要宏。

### 3.3 最佳实践清单

1. "只该编译期发生"的逻辑（造表、常量校验）一律 `consteval` 锁死；
   通用工具函数保持 `constexpr` 双模
2. 全局/静态**可变**状态声明 `constinit`——初始化时序交给编译器背书
   （只读的直接 `constexpr`）
3. 位置信息默认实参化：接口写 `loc = std::source_location::current()`，
   调用方零负担
4. 立即函数里不要写依赖运行环境的分支（locale、随机、时钟）——
   常量求值实际执行到不允许的操作时会报错
5. 用 `constexpr` 变量接住造表结果可表达只读常量；是否放入特定只读段、
   是否完全消除存储由实现和优化决定，语言不保证二进制布局

## 常见陷阱

1. **以为 consteval 只能写在 constexpr/static_assert 中**：
   `int n = sq(2);` 完全合法，sq(2) 仍须常量求值；传运行期实参才会失败
2. **constinit 当 const 用**：它不锁写只锁初始化方式；要只读加 `const`
   或改 `constexpr`
3. **自动存储期局部变量写 constinit**：编译错；函数内的 static 或
   thread_local 变量可以使用 constinit
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
   解释它为何不依赖动态初始化顺序，再说明 constinit 为何不解决数据竞争
   或其他全局对象的依赖（对照第 26 课）。

## 参考

- [C++20 草案：constinit 的存储期与初始化规则](https://timsong-cpp.github.io/cppwp/n4861/dcl.constinit)
- [C++20 草案：立即调用与常量表达式](https://timsong-cpp.github.io/cppwp/n4861/expr.const)
- 提案 P1073R3（consteval 立即函数）/ P1143R2（constinit）/
  P1208R6（std::source_location）
- cppreference: [consteval](https://zh.cppreference.com/w/cpp/language/consteval)
- cppreference: [constinit](https://zh.cppreference.com/w/cpp/language/constinit)
- cppreference: [std::source_location](https://zh.cppreference.com/w/cpp/utility/source_location)

## 补充阅读

[40A · 常量求值与编译期、运行时双路径](../补充知识点/40_补充A_常量求值与双路径/40_补充A_常量求值与双路径.md)。练习在同一个 constexpr 接口中选择常量求值与运行时实现。
