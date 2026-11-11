# C++17 补充知识点

本文件夹集中存放 C++17 的补充讲解与配套示例。沿用 C++14 的组织方式：
每节一个独立子文件夹，包含同名 Markdown 和 ASCII 文件名的 C++ 示例。
通常可单文件编译；26A 使用一个头文件与两个 cpp，实际验证跨翻译单元的实体身份。
编号表示接在哪节主线课后阅读，不占每日主线编号，也不改变自动生成计划。

目前主线已到第 27 课。下表共 17 节补充课，状态均为“课件已生成，待学习”。

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
| 20A | 第 20 课后 | [字符串视图的所有权与长度边界](20_补充A_字符串视图的所有权与长度边界/20_补充A_字符串视图的所有权与长度边界.md) | [20a_view_boundaries.cpp](20_补充A_字符串视图的所有权与长度边界/20a_view_boundaries.cpp) |
| 21A | 第 21 课后 | [路径规范化与分层错误处理](21_补充A_路径规范化与分层错误处理/21_补充A_路径规范化与分层错误处理.md) | [21a_path_and_errors.cpp](21_补充A_路径规范化与分层错误处理/21a_path_and_errors.cpp) |
| 22A | 第 22 课后 | [属性诊断的边界与副作用](22_补充A_属性诊断的边界与副作用/22_补充A_属性诊断的边界与副作用.md) | [22a_attribute_diagnostics.cpp](22_补充A_属性诊断的边界与副作用/22a_attribute_diagnostics.cpp) |
| 23A | 第 23 课后 | [多对象一致快照与锁边界](23_补充A_多对象一致快照与锁边界/23_补充A_多对象一致快照与锁边界.md) | [23a_consistent_snapshot.cpp](23_补充A_多对象一致快照与锁边界/23a_consistent_snapshot.cpp) |
| 24A | 第 24 课后 | [完整解析与结果提交](24_补充A_完整解析与结果提交/24_补充A_完整解析与结果提交.md) | [24a_strict_charconv.cpp](24_补充A_完整解析与结果提交/24a_strict_charconv.cpp) |
| 25A | 第 25 课后 | [归约规则与执行策略边界](25_补充A_归约规则与执行策略边界/25_补充A_归约规则与执行策略边界.md) | [25a_reduction_contracts.cpp](25_补充A_归约规则与执行策略边界/25a_reduction_contracts.cpp) |
| 26A | 第 26 课后 | [跨文件实体身份与 ODR](26_补充A_跨文件实体身份与ODR/26_补充A_跨文件实体身份与ODR.md) | [主程序](26_补充A_跨文件实体身份与ODR/26a_odr_identity.cpp) + [第二 TU](26_补充A_跨文件实体身份与ODR/26a_odr_peer.cpp) |
| 27A | 第 27 课后 | [临时对象物化与引用生命周期](27_补充A_临时对象物化与引用生命周期/27_补充A_临时对象物化与引用生命周期.md) | [27a_temporary_lifetimes.cpp](27_补充A_临时对象物化与引用生命周期/27a_temporary_lifetimes.cpp) |

按表格顺序读完对应主线后，再读同编号的补充课。
11A 的自定义分解部分含一个简短的 `if constexpr`，初读可先看引用语义，
学完第 13 课后再回看该部分。

11A～14A 补充引用、作用域、能力检测和折叠求值的细节。
15A～19A 从类型推导与初始化出发，分别练习惰性兜底、多 variant 访问、动态类型注册，
最后在一个解码入口组合使用 optional、variant 和 any。
20A～23A 进一步练习字符串借用边界、文件系统路径与错误处理、属性诊断，
以及跨对象共享读取的一致快照。
24A～27A 练习完整数值字段校验、可合并的归约结果、跨文件共享实体，
以及临时对象物化后的引用生命周期。

每节编译命令均从 `ModernCPP` 仓库根目录执行。
更多 C++14 前置知识见 [C++14 补充目录](../../CPP14/补充知识点/README.md)。

返回 [现代 C++ 课程总目录](../../README.md)。
