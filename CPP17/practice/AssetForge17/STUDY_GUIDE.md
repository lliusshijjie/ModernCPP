# AssetForge17 学习指南

先用 [README](README.md) 中的命令跑完示例。全 20 课的映射表在 [工程设计](DESIGN.md) 中。沿着下面的顺序阅读，可以分别跟踪名称解析、字节所有权和构建结果。

## 1. 从清单跟到包内序号

从 [load_catalog](src/manifest.cpp) 开始。清单每行先分成 `string_view` 字段，记录里的字符串再由目录拥有。第一遍登记所有稳定 ID，第二遍才解析依赖、别名和配置，所以 `requires` 在 `asset` 之前也合法。给示例里的 `ui.start` 增加一个不存在的依赖，查看错误是否指向那一行。

在 [plan 和 topological_order](src/planner.cpp) 里追踪 `ui.start`。闭包找到三个资源，最小堆从无剩余依赖的资源中挑 ID 最小者。想一想若把 `ui.theme` 改名为 `ui.accent`，内容区偏移会如何变化，再实际运行 `plan` 核对。

## 2. 先算字节，再看编码器

从 [prepare_asset](src/builder.cpp) 进入 `std::visit → std::apply → prepare`，区分源文件的字节数与行尾处理后的字节数。把零字节上限分别给一个空资源和一个非空资源，看看 `optional<Size>` 的两个状态如何参与判断。

沿 [write_bundle 和 BundleReader](src/bundle.cpp) 计算 32 字节包头。资源条目偏移从内容区起点算起。读者可以用 `header_size + index_size + entry.offset` 定位内容。运行 `ctest --test-dir build -R "^bundle$" -V`，对照单条资源 `x` 的 68 字节固定样本，分别找出索引 CRC 和数据 CRC。

## 3. 确定并行边界

阅读 [checksums](src/io.cpp)。构建器先准备资源缓冲区和块描述，校验器也在得到稳定缓冲区后才开始计算。逐项写出回调读了什么、结果写到哪里，以及哪个阶段负责 I/O 失败。

命令行测试将声明顺序反转、把源文件移到另一个目录，再切到 `par` 构建。对照 SHA-256 相等，说明这些改变不影响 AFP 文件。也可以只改变某个资源的原始内容，预先判断会变化的长度、后续偏移和 CRC。

## 4. 跟踪一次改名冲突

在 [Workspace](src/workspace.cpp) 中，写入者同时持锁确认选择和目录相容。读者复制完快照后释放锁，旧快照仍可以继续计算旧计划。

在清单里给 `ui.logo` 添加别名 `logo`，然后重载会话并执行 `rename-alias startup logo`。观察 `insert` 失败时返回的节点如何换回原名。成功改名后，稳定 ID 和依赖序号仍表示同一资源。最后读 [ReportRegistry](include/assetforge/report.hpp)，看一个拥有型报告值怎样通过 `std::any` 到达与它配对的呈现函数。

## 练习

1. 给样例新增第二个入口，使两个入口共享 Logo。先手算多根配置的完整顺序，再在程序中核对共享资源只打包一次。
2. 加一个只统计零字节资源的自定义报告。跟踪推导的报告值类型，解释注册表为什么能调用成员函数指针。
3. 用固定格式样本改变一个依赖序号，更新索引 CRC 后再打开它，分别检查 CRC 验证和结构约束的作用。
