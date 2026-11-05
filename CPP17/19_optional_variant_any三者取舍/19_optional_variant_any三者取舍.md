# 第 19 课 · 综合：optional / variant / any 三者取舍（C++17）

> 所需标准：**C++17** ｜ 讲解：本文件 ｜ 独立代码：`19_optional_variant_any.cpp`
> 前置：第 16 课 optional、第 17 课 variant、第 18 课 any

## 一、底层原理

### 1.1 三个类型 = 三种"类型不确定性"的表达

这不是三个功能重叠的工具，而是**不确定性程度的三个刻度**：

| | 回答的问题 | 类型集合 | 不确定性 |
|---|---|---|---|
| `optional<T>` | 有还是没有？ | 固定 T，0 或 1 个 | **是否存在** |
| `variant<Ts...>` | 是候选中的哪个？ | 编译期**封闭**列举 | 是 A 还是 B（有限） |
| `any` | 装的到底是啥？ | **开放**，运行期才知道 | 完全擦除 |

选型的本质：**把不确定性压到最小刻度**。能用 optional 不用 variant，
能用 variant 不用 any——每升一级，编译器替你检查的东西就少一分。

### 1.2 存储与机制对比（开销阶梯）

| | 存储布局 | 取值机制 | 拷贝语义 |
|---|---|---|---|
| optional\<T\> | 栈上 T + bool 标记 | 直接访问，零间接 | 拷贝 T |
| variant\<Ts...\> | 栈上 union（≈最大成员）+ 下标 | get/visit 按**编译期已知**的候选分派 | 拷贝当前活跃成员 |
| any | 类型擦除：管理器指针 + 小缓冲（装不下就**堆分配**） | `any_cast` 走 **RTTI 比对** | 可能触发堆分配的深拷贝 |

- optional/variant 的"是什么"在**类型系统里**，编译器全程代管
- any 的"是什么"在**运行期数据里**，类型信息被擦除，取回要靠 typeid 比对
- 开销与灵活性的交易一目了然：栈上零间接 → 栈上带标签 → 可能堆 + RTTI

### 1.3 安全性对比：错误暴露的时机

| 错误用法 | optional | variant | any |
|---|---|---|---|
| 空/错型取值 | `*` 空 = UB（快）或 value() 抛 | `get<T>` 抛 bad_variant_access | `any_cast<T>` 抛 bad_any_cast |
| 忘记处理某种情况 | if 忘判断 → 运行期炸 | **visit 穷尽性编译期把关** | 无任何编译期保护 |
| 类型拿错 | 编译错（类型写死） | 编译错或运行期抛 | **运行期才抛** |

variant 的 visit 穷尽性检查是三件套里唯一的编译期安全网——这是它
常常优于 any 的决定性理由。

## 二、具体语法

### 2.1 对比速查表

| 操作 | optional\<T\> | variant\<Ts...\> | any |
|---|---|---|---|
| 创建 | `o = val` / `nullopt` | `v = alt` | `a = val` |
| 判有 | `has_value()` / `if (o)` | `!valueless_by_exception()`（几乎恒真） | `has_value()` |
| 判型 | —（类型固定） | `holds_alternative<T>()` | `a.type() == typeid(T)` |
| 安全取值 | `value()` 抛 / `value_or` | `get<T>` 抛 / `get_if` 不抛 | `any_cast<T>` 抛 / `any_cast<T>(&a)` 不抛 |
| 分派处理 | if | `visit` + overloaded | if-else 链手写 any_cast |

### 2.2 选型决策规则

1. **"可能没有"** → optional（值域里没有合适的魔法值时尤其）
2. **"有限候选、逐个处理"** → variant（享受 visit 穷尽性检查）
3. **"类型集合开放/由别人决定"** → any（插件、属性袋、跨 ABI 边界）
4. 候选类型相同只是语义不同 → 强类型包装 + variant，不用 any
5. 需要长期存储 + 高频访问的动态类型 → 考虑 variant 专门化或继承，
   any 的 RTTI 比对与堆分配是热路径负担

## 三、实际应用与最佳实践

### 3.1 场景：配置系统三层用对三件套（示例 ③）

- **optional**：可选字段（`optional<int> port`——没给就 `value_or` 默认）
- **variant**：值类型封闭已知（`variant<bool, string> debug`——开关或级别）
- **any**：开放扩展槽（`map<string, any> plugin_extra`——插件想存啥存啥）

同一个系统里三者各司其职——这是它们在真实工程里的标准合影。

### 3.2 最佳实践清单

1. 选型从低刻度往高刻度试：optional → variant → any
2. any 取回值统一走 `any_cast<T*>(&a)` 判空指针风格，别到处 try/catch
3. any 装的自定义类型跨编译单元/ABI 时留意 RTTI 一致性
4. optional 的消费优先 `value_or`；variant 的消费优先 `visit`
5. 三者都可放容器：`vector<optional<T>>`/`vector<variant<...>>` 完全合法，
   `vector<any>` 合法但先问自己是不是该用 variant

## 常见陷阱

1. **any_cast 必须精确匹配**：存的是 `int`，`any_cast<long>` 就抛——
   没有隐式转换；存 `short` 取 `int` 也一样失败
2. any 的拷贝可能堆分配深拷贝——按值传参/放容器的成本要先想清楚
3. 用 any 解决本该 variant 的问题：丢失穷尽性检查，新增候选类型时
   所有 if-else 链**静默漏改**（编译器帮不上忙）
4. variant 默认构造是第一个备选，any 默认构造是空——两套"空"语义，
   别混着记（optional 的空才是三件套里语义最直观的）
5. `optional<T>` 传参表达"可省略"不如默认实参/重载直观（第 16 课清单重申）

## 完整示例

见 `19_optional_variant_any.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++17 -Wall -Wextra "CPP17/19_optional_variant_any三者取舍/19_optional_variant_any.cpp" -o run && ./run
```

## 练习

1. 给示例配置系统的 `plugin_extra` 写 `get_as<T>(key, def)` 模板：
   any 存在且类型匹配返回值，否则返回默认——体会 any_cast 的指针风格。
2. 用 `variant<optional<int>, string>` 建模"可能是缺失、可能是数字、可能是
   文本"的 CSV 单元格，写 parse + render；再想想有没有更好的建模。
3. 分 别用 any 与 variant 实现"设置值"（int/bool/string 三种），
   故意漏处理一种类型：观察 variant 版编译报错、any 版静默通过——
   把两种结局写进团队规范。
4. 用 `static_assert(sizeof(...) > ...)` 锁住三件套的大小关系
   （optional<int> < variant<int,string>），跨平台 CI 里兜底。

## 参考

- cppreference: [std::any](https://zh.cppreference.com/w/cpp/utility/any)
- C++ Core Guidelines C.183 / TV.5（类型安全与联合体）
- 第 16/17/18 课——本课是它们的三合一复盘

## 学习衔接

继续阅读 [补充 19A · 缺失值与错误的分层建模](../补充知识点/19_补充A_缺失值与错误的分层建模/19_补充A_缺失值与错误的分层建模.md)。
