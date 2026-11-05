# C++17 补充知识点

本文件夹集中存放 C++17 的补充讲解与配套示例。沿用 C++14 的组织方式：
每节一个独立子文件夹，包含同名 Markdown 和 ASCII 文件名的独立 C++ 示例。
编号表示接在哪节主线课后阅读，不占每日主线编号，也不改变自动生成计划。

目前主线已到第 19 课。下表共 9 节补充课，状态均为“课件已生成，待学习”。

| 编号 | 推荐阅读位置 | 补充内容 | 独立示例 |
|---|---|---|---|
| 11A | 第 11 课后 | [结构化绑定的引用与自定义分解](11_补充A_结构化绑定的引用与自定义分解/11_补充A_结构化绑定的引用与自定义分解.md) | [11a_binding_protocol.cpp](11_补充A_结构化绑定的引用与自定义分解/11a_binding_protocol.cpp) |
| 12A | 第 12 课后 | [初始化语句与RAII生命周期](12_补充A_初始化语句与RAII生命周期/12_补充A_初始化语句与RAII生命周期.md) | [12a_init_raii.cpp](12_补充A_初始化语句与RAII生命周期/12a_init_raii.cpp) |
| 13A | 第 13 课后 | [void_t检测与编译期分支边界](13_补充A_void_t检测与编译期分支边界/13_补充A_void_t检测与编译期分支边界.md) | [13a_detection_and_constraints.cpp](13_补充A_void_t检测与编译期分支边界/13a_detection_and_constraints.cpp) |
| 14A | 第 14 课后 | [折叠表达式的顺序与短路](14_补充A_折叠表达式的顺序与短路/14_补充A_折叠表达式的顺序与短路.md) | [14a_fold_sequencing.cpp](14_补充A_折叠表达式的顺序与短路/14a_fold_sequencing.cpp) |
| 15A | 第 15 课后 | [CTAD 推导与初始化](15_补充A_CTAD推导与初始化/15_补充A_CTAD推导与初始化.md) | [15a_ctad_initialization.cpp](15_补充A_CTAD推导与初始化/15a_ctad_initialization.cpp) |
| 16A | 第 16 课后 | [optional 惰性兜底与值语义](16_补充A_optional惰性兜底与值语义/16_补充A_optional惰性兜底与值语义.md) | [16a_optional_lazy_fallback.cpp](16_补充A_optional惰性兜底与值语义/16a_optional_lazy_fallback.cpp) |
| 17A | 第 17 课后 | [多 variant 访问与状态转移](17_补充A_多variant访问与状态转移/17_补充A_多variant访问与状态转移.md) | [17a_multi_variant_visit.cpp](17_补充A_多variant访问与状态转移/17a_multi_variant_visit.cpp) |
| 18A | 第 18 课后 | [any 的类型处理器注册](18_补充A_any的类型处理器注册/18_补充A_any的类型处理器注册.md) | [18a_any_handler_registry.cpp](18_补充A_any的类型处理器注册/18a_any_handler_registry.cpp) |
| 19A | 第 19 课后 | [缺失值与错误的分层建模](19_补充A_缺失值与错误的分层建模/19_补充A_缺失值与错误的分层建模.md) | [19a_decode_layers.cpp](19_补充A_缺失值与错误的分层建模/19a_decode_layers.cpp) |

按表格顺序读完对应主线后，再读同编号的补充课。
11A 的自定义分解部分含一个简短的 `if constexpr`，初读可先看引用语义，
学完第 13 课后再回看该部分。

11A～14A 补充引用、作用域、能力检测和折叠求值的细节。
15A～19A 从类型推导与初始化出发，分别练习惰性兜底、多 variant 访问、动态类型注册，
最后在一个解码入口组合使用 optional、variant 和 any。

每节编译命令均从 `ModernCPP` 仓库根目录执行。
更多 C++14 前置知识见 [C++14 补充目录](../../CPP14/补充知识点/README.md)。

返回 [现代 C++ 课程总目录](../../README.md)。
