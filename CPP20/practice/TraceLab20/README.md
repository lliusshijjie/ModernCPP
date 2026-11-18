# TraceLab20 · 本地日志分析引擎

把几个组件的 UTF-8 日志交给同一个命令，就能看到同一时间轴上的事件、各组件的命中数和耗时最长的操作。每条明细保留文件路径和物理行号。这个项目串联 [C++20 第 31～48 课](docs/COURSE_MAP.md)，只依赖标准库。

## 第一次运行

在本目录用 PowerShell 执行以下命令。本机 Windows 验证使用 MSYS2 GCC 15.2、Ninja 和 CMake 4.2。模块目标需要 CMake 3.28 以上及支持模块扫描的生成器和编译器。

~~~powershell
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++ -DTRACELAB_MODULES=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 4
ctest --test-dir build --output-on-failure
.\build\tracelab.exe analyze --input data/demo.log --min-level warn --component network --top 1
~~~

这个查询匹配两条网络事件。时间线只显示最早的一条，即第 3 行的“等待响应”。最慢的一条是第 2 行的“连接重试”。统计仍包含两条记录，平均耗时为 75,000 微秒。用 `--latest` 可以让时间线从最新记录开始。

~~~text
processed=4 invalid=0 matched=2 flag_bits=3
timed=2 total_us=150000 min_us=0 max_us=150000 mean_us=75000.00
~~~

模块版的可执行文件是 `tracelab_modules`，可以运行相同命令。WSL 上可用默认的头文件构建。标准库须支持 C++20 format、协程、原子等待和 chrono 时区接口。命名时区用例还需要可访问的时区数据库。

~~~sh
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux -j 4
ctest --test-dir build-linux --output-on-failure
./build-linux/tracelab analyze --input data/demo.log
~~~

## 用自己的日志

输入使用 [七列 Tab 协议](docs/FILE_FORMAT.md)。可以重复指定 `--input`，目录默认枚举当前层的 `.log` 普通文件。显式指定的文件可以使用其他扩展名。文件在扫描期间应保持稳定。完整命令行选项见 `tracelab --help`。

~~~powershell
.\build\tracelab.exe analyze --input data/demo.log --from 2026-11-18T01:31:00Z --keyword 重试 --flags 3
.\build\tracelab.exe analyze --input data/demo.log --workers 4 --timezone Asia/Shanghai --progress --report build/report.txt --snapshot build/report.tl20
.\build\tracelab.exe show-snapshot build/report.tl20 --timezone UTC
.\build\tracelab.exe analyze --input data --strict
.\build\tracelab.exe analyze --input data/demo.log --timeout 0ms
~~~

时间区间为左闭右开，组件集合中的任一项可以命中，标记掩码要求指定的位全部存在。关键词按 UTF-8 原文区分大小写匹配。时间桶按 UTC 自然分钟划分，显示时才转换时区。可以使用 `+08:00` 这样的固定偏移。

统计中的 `processed` 是已提交批次的非空行数，包含坏行。`matched` 只计有效且通过全部筛选的事件。`flag_bits` 是所有匹配事件的标记置位数之和。`duration_us` 为零的事件参与平均值，为 `-` 的事件只参与事件统计。

| 分析退出码 | 含义 |
|---|---|
| 0 | 完整扫描，所有非空行有效，允许零匹配 |
| 1 | 完整扫描，遇到坏行并跳过，按文件和行号最多列出 100 条诊断 |
| 2 | 失败，例如严格模式下遇到坏行、I/O 失败、分组超限或计数溢出 |
| 3 | 收到取消请求，输出标有 PARTIAL 的已提交批次统计 |

`show-snapshot` 成功读取并展示返回 0。快照保存查询说明、计数、范围和分组，用于换时区重看统计。时间线、最慢事件及逐行诊断保存在文本报告中。两个输出文件分别在同目录写完后替换。取消时保留已有快照。

## 并发与资源界限

调用线程从协程取出记录，再把拥有数据的批次移动进有界队列。每个工作线程拥有一份统计，处理完整批次后提交。读到 EOF 时队列关闭入口并排空，所有工作线程退出后合并结果。相同时间的事件按规范化路径的序号和行号定序。可用 `analyze_serial` 从同一输入取得顺序结果。

每批最多 256 条、1 MiB 原始记录，队列最多容纳工作线程数的两倍批次。每条输入限 64 KiB，文件清单限 10,000 个路径，累计结果限 1,024 个组件和 10,000 个非空分钟桶。时间线与耗时榜分别最多保留 1,000 条，消息预览限 512 字节，筛选使用完整消息。

进度按批次发布到 stderr。超时从分析启动时计时，发起协作取消。部分报告覆盖哪些批次由提交时机决定。普通文件 I/O 完成后才能继续检查停止令牌。

学习入口见 [逐步阅读与练习](STUDY_GUIDE.md)、[18 课源码对照](docs/COURSE_MAP.md) 和 [原始设计](DESIGN.md)。
