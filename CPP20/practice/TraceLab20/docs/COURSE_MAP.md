# C++20 课程与实现

| 课 | 阅读位置 | 可以检查的行为 |
|---|---|---|
| 31 Concepts | [Accumulator 与 accumulate](../src/analysis.cpp) | 接受具有 accept、merge 操作的累计器，以 input_range 为输入。正反 static_assert 检查类型约束 |
| 32 Ranges 算法 | [finish](../src/analysis.cpp) | sort 对诊断使用来源位置投影，输出顺序可重复 |
| 33 惰性视图 | [analyze_batch](../src/analysis.cpp) | filter 引用本批事件，遍历匹配项时才调用查询谓词 |
| 34 视图组合 | [CLI](../src/cli.cpp)、[参考结果](../tests/self_test.cpp) | split 拆组件列表，take 与 transform 取得全量排序后的前 K 个键，iota 生成测试输入 |
| 35 三路比较 | [Position 和 Key](../include/tracelab/model.hpp) | 同一 UTC 时间再比较文件序号和物理行 |
| 36 format | [文本报告](../src/report.cpp) | 等级列对齐，耗时均值保留两位，时间含微秒与偏移 |
| 37 span | [批次分析](../src/analysis.cpp)、[Reader](../src/snapshot.cpp) | 接收一段连续记录或剩余字节，每次切片前检查可读长度 |
| 38 jthread 与 stop_token | [运行时](../src/session.cpp)、[有界队列](../include/tracelab/queue.hpp) | 生产者与消费者的阻塞等待都能收到同一个会话的停止请求 |
| 39 协程 | [生成器](../include/tracelab/generator.hpp)、[文件源](../src/input.cpp) | 按需打开文件、产出一行、移交异常、提前销毁帧 |
| 40 consteval、constinit、source_location | [字段表](../include/tracelab/schema.hpp)、[会话编号和完整性检查](../src/session.cpp) | 字段表在编译时构造，编号允许并发递增，内部计数异常可定位到调用点 |
| 41 模板 lambda、指定初始化、分支属性 | [parse 与 records](../src/input.cpp) | 模板 lambda 保留各解码器的返回类型，Key 按成员名初始化，行读取提示常见分支 |
| 42 bit 家族 | [统计](../src/analysis.cpp)、[快照](../src/snapshot.cpp) | popcount 计算置位数，bit_cast 保留整数与浮点表示，endian 决定字节顺序 |
| 43 统一擦除 | [render_statistics](../src/report.cpp) | erase_if 从展示列表移除零计数的等级 |
| 44 constexpr 容器 | [schema](../include/tracelab/schema.hpp) | 在常量求值期间用 string/vector 规范化并查重，返回固定数组。配置测试尝试编译重复字段 |
| 45 atomic 等待与通知 | [ProgressBus](../src/session.cpp) | 观察线程等待版本变化，最后一次发布携带 done 标志 |
| 46 char8_t 与 UTF-8 | [输入边界](../src/input.cpp)、[Windows 入口](../src/cli.cpp) | UTF-8 路径、关键词和消息验证，预览在码点边界截断 |
| 47 Modules | [接口](../modules/tracelab.model.cppm)、[导入方](../modules/main.cpp) | 命名模块重新导出已有模型与 CLI 接口，构建可执行文件并运行同一份 CLI 验收 |
| 48 日历与时区 | [时间解析](../src/input.cpp)、[Zone](../src/report.cpp) | year_month_day 校验日期，UTC 分桶。测试用纽约回拨日的两个 01:30 验证偏移不同 |
