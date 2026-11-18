# 跟踪一条记录和一次取消

先运行 README 中的第一条查询，再用下面的顺序读代码。每一步都可以通过更改一个参数或运行一个测试核对自己的判断。

## 1. 在纸上计算一份报告

打开 [demo.log](data/demo.log)，把四条记录的时间统一到 UTC。第 1 行写的是东八区的 09:30，对应 UTC 01:30。第 2、4 行表示同一瞬间，平局时行号较小的在前。去掉筛选后，时间线的行号顺序应当是 1、3、2、4。完整的预期输出在 [expected_report.txt](data/expected_report.txt)，其中 `<demo>` 代表本机的规范化文件路径。

再加 `--top 0`。明细变空，三个带耗时事件的平均值仍然是 66,666.67 微秒。读 [Statistics 与 retain](src/analysis.cpp)，找出计数更新和候选淘汰发生的位置。

## 2. 找出三个所有者

从 [records](src/input.cpp) 追到 [produce](src/session.cpp)。协程帧里的 Record 在挂起时还活着。生产者将记录移动到批次，迭代器推进后，协程可以复用原先的对象。随后，工作线程从队列得到整个批次。

打开 [analyze_batch](src/analysis.cpp) 对照 [字段解析 parse](src/input.cpp)。parse 中的 string_view 指向当前行。Event 的消息已经拥有自己的字节。惰性筛选视图引用本批的 Event，而最终候选又拥有一份最多 512 字节的预览。试着指出这些对象各自最早可以在哪一步销毁。

实验命令为 `ctest --test-dir build -R "^input$" -V`。测试还会消费一条记录后销毁生成器，然后删除文件，可在 Windows 上观察句柄已经关闭。

## 3. 手算一次堆合并

取 K=2。假设两个线程分别遇到耗时 3、20、8 和 10、20、1。写出各自保留的两个候选，再手工合并，最后用来源键解开 20 的平局。读 [retain、slower 和 merge](src/analysis.cpp)，找出堆顶为何是当前最先可能被淘汰的元素。

在 [analysis_checks](tests/self_test.cpp) 中，参考结果保存全部匹配事件，排序后用 take 和 transform 取得前 11 个键。它可以独立检验堆的选择与顺序。把最慢事件的平局比较方向改反，重新构建，然后运行 `ctest --test-dir build -R "^analysis$" --output-on-failure` 观察失败，随后恢复比较方向并重新构建。

## 4. 观察队列的两种结束方式

跟读 [analyze](src/session.cpp) 和 [BoundedQueue](include/tracelab/queue.hpp)。按正常 EOF 顺序标出最后一批提交、队列关闭、工作线程 join、结果合并的位置。再假设队列已满，消费者解析一条记录时抛出异常。跟踪 fail 请求停止后，正阻塞在 push 中的生产者如何醒来。

运行 `analyze --input data/demo.log --timeout 0ms --progress` 可稳定得到取消状态。对大文件使用非零超时可观察已提交的批次。进度回调在独立线程运行，收到 done 后退出。读 ProgressBus 时，先画出“取得版本、复制进度、等待版本变化”的时序，再尝试把发布操作插入任意两个步骤之间。

## 5. 分开验证格式和业务含义

对 [快照解码器](src/snapshot.cpp) 的一次读取画出游标位置。长度足够只代表字段读得出来，还要检查分组之和等于匹配数、耗时范围与总量相容、时间范围与首尾分钟桶相符。运行 `ctest --test-dir build -R snapshots -V` 可以看到测试结果。测试对一份有效快照的每一个截断位置都尝试解码。

最后读 [命名模块入口](modules/main.cpp) 和 [模块接口](modules/tracelab.model.cppm)。两种 CLI 链接同一套分析实现，分别运行相同的命令行验收脚本。可以用 `ctest --test-dir build -R cli_ -V` 对照。

## 独立练习

1. 新增 PID 筛选。先写一个恰好有两条同组件、不同 PID 的例子，再沿参数解析、Query 和查询摘要追踪改动。文件格式已经保存 PID。
2. 新增每个组件的最大耗时。先确定全组数据都缺失耗时时的输出，再写出两个局部结果的合并规则。
3. 为三个文件构造同时间、同耗时事件，用不同输入参数顺序和线程数验证报告排序。说明单凭“稳定排序”还缺少哪个前提。
