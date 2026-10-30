# 阅读路线与动手练习

目标是能解释数据如何从输入流进入对象、如何跨线程存活、如何成为一份类型化报告。
先读 [项目说明](README.md) 并运行一次，再按下面的步骤推进。

## 第一遍：只追一条 pulse 任务

打开 [main.cpp](src/main.cpp)，从无参数分支进入 `run`。

1. `read_series` 返回 `vector<unique_ptr<Series>>`：容器拥有指针，指针拥有采样对象。
2. `catalog.upsert(std::move(*series))` 把对象内容转移给目录，原指针仍拥有被移出的对象。
   `loaded.clear()` 随后销毁这些对象；代码不再读取它们的旧内容。
3. `config.try_snapshot` 取得一份 `Settings` 值；解锁后也可以安全使用。
4. `catalog.snapshot(name.c_str())` 在读锁内复制对象，返回独占指针。
5. 默认演示拿到 `selected(1)` 的代理并修改它；目录中的原始掩码没有变化。
6. `executor.submit(std::move(snapshot), settings)` 把采样指针和配置移入闭包。
7. 队列把任务交给工作线程；`analyze` 使用闭包中仍然存活的数据。
8. `shutdown` 处理完已接收任务、join 工作线程；`future.get()` 再取结果或抛出任务异常。
9. 主线程排序并打印报告，所以输出顺序与线程调度无关。

先自行回答：哪一步释放锁？哪一步复制采样？哪一步仅转移指针？
在纸上画出三个不同对象：目录中的 Series、任务快照 Series、最后的 Report。

## 第二遍：读数据类型，确认接口返回什么

打开 [model.h](include/signal_lab/model.h)。

### 1. 编译期表不是可写 std::array

`make_gain_table` 的局部变量、循环、赋值属于 C++14 constexpr 增强。
为了使用 C++14，本项目用一个内建数组包装固定表；没有调用 C++17 才 constexpr 化的
`std::array` 可写下标。`static_assert` 把期望值放进必须常量求值的上下文。

检查点：把倍率初值从 1 改为 2，哪条静态断言先失败？
如果把同一个函数用于普通运行期初始化，它仍不因此保证编译期执行。

### 2. 聚合的默认值会补齐未提供字段

`Settings{"double", 1}` 显式提供标题与倍率索引，功能位和超时使用默认成员初值。
不是给每种参数组合分别写一个构造函数。

检查点：先预测这份配置的 `features` 和 `lock_budget`，再对照声明。

### 3. 三种“看起来都像一个值”的返回

| 表达式 | 实际类型 | 是否依赖原对象 |
|---|---|---|
| `series.samples()` | `const vector<double>&` | 是，借用样本容器 |
| 非 const `series.selected(i)` | `vector<bool>::reference` 的值 | 是，代理访问原位 |
| const `series.selected(i)` | `bool` | 否，独立布尔值 |

`auto proxy = series.selected(i)` 的按值推导保留了代理类型。
要取得独立布尔快照，写 `bool selected = series.selected(i)`。
不要在 Series 销毁后继续使用其引用或代理。

getter 的 `const && = delete` 禁止从临时 Series/Report 直接借出上述成员。
它不能替你检查所有间接生命周期；例如临时 unique_ptr 销毁后，其对象的引用也会悬空。

### 4. 两个 string 字段为什么能按类型取

`ReportRow` 的类型依次为：`SignalName`、`BatchTitle`、`Summary`、`Spectrum`、`microseconds`。
两个文本字段都使用语义包装，所以类型键唯一。

检查点：把两个包装直接改成 `std::string`，再思考 `get<std::string>` 为什么不再可用。
`Report::summary()` 返回引用，但普通 `auto copy = report.summary()` 会复制 Summary；
`decltype(auto) alias = report.summary()` 才保留引用。

## 第三遍：理解导入、索引与读写锁

打开 [workbench.cpp](src/workbench.cpp)。

### 1. 输入解析与对象验证分工

- `read_series` 检查文件结构、计数、重复名称和多余字段。
- `Series` 构造函数检查对象的名称、数量、有限性与幅值范围。
- `validate_settings` 也被提交和分析路径调用，检查构造后被修改的配置。

检查点：为什么 `"x" 1.0` 需要专门检查计数后面的字符？
仅使用流的整数提取，可能先取得计数 1，再把 `.0` 当成下一个样本。

`std::quoted` 是字段的转义/提取工具。本项目另外检查开头是双引号，
因为 quoted 默认也允许读取一个无引号单词。

### 2. SFINAE 的工作位置

`make_series` 的模板参数使用 `enable_if_t`，只有元素类型满足要求的范围才成为候选。
`sample_type_t` 中的 `decltype(*cbegin(...))` 在不求值上下文中检查类型。
它不需要实际创建一个范围对象。

这与在函数体中写 `static_assert` 不同。打开自检中的 `can_make_series`，
观察 bool 范围、整数对象为何能在候选筛选阶段被检测为不可调用。
模板还会接受字符的数值；输入文件中的数字文本则由流解析，它们不是同一种操作。

### 3. 持锁时借用，解锁后持有副本

`Catalog::snapshot` 用共享锁保护查询与复制，返回后不再借用 map 内的对象。
`ConfigStore::reload` 在锁外解析和验证，成功后才在独占锁内整体替换。

检查点：为什么不能把 `snapshot` 改成返回 `const Series&`，然后仍立即释放读锁？
另一个线程可能替换该对象；调用者就会在锁外访问正在变化的数据。

`less<>` 让 `find(const char*)` 使用异构查找。map 的 string 键仍然拥有字符数据，
查询参数不会被保存进容器，也不会因此延长生命周期。

### 4. 比较完整范围与比较公共前缀

`compare_rosters` 三次都传入双方的起止迭代器。
`equal` 比较完整长度和对应元素，`is_permutation` 比较元素及重复次数，
`mismatch` 找首个差异或任一端点。本项目只计算它与 begin 的距离，不盲目解引用结果。

检查点：`{"a","b"}` 与 `{"a","b","c"}` 的三个返回字段分别是什么？

## 第四遍：计算无需先学数字信号处理

统计量只对选中点进行校准：

- 校准值 = 输入值 × 倍率表中的值。
- 平均值 = 选中值之和 ÷ 选中数量。
- RMS = 所有选中值平方的平均值，再开平方。

复数部分只计算一个分量：

```text
X1 = sum(x[n] * exp(-i * 2*pi*n/N)), n=0..N-1
```

N 是原采样长度，未选中的位置令 x[n]=0，不把它后面的点向前移动。
`phase` 从 `1 + 0i` 开始，每次乘固定的 `step`，得到下一个位置的复数权重。
这给复数字面量一个实际用途；它不是完整频谱分析工具。

检查点：对 `[1,0,-1,0]`，四个权重约为 `[1,-i,-1,i]`，手算 X1 应得到 2。
默认演示关闭的是一个零值点，所以 X1 不变，但 RMS 的分母从 4 变成 3。

## 第五遍：跨线程生命周期与关闭

打开 [executor.cpp](src/executor.cpp)。

### 1. 移动捕获改变谁拥有数据

`owned = std::move(series)` 使闭包拥有独占指针；它的类型不可拷贝。
因此队列使用 `packaged_task<Report()>`，没有强行把任务塞入要求目标可拷贝的 `std::function`。
任务不保存 submit 调用栈上某个临时变量的地址。

配置也按值捕获，所以后续更新 ConfigStore 不会让同一批任务读到不同倍率。
任务执行时使用的是提交时的配置。

### 2. 等待有条件，执行不持锁

`ready_.wait` 的谓词在锁内检查“正在关闭或存在任务”。
取出任务后释放队列锁，再进行计算；这样不同工作线程才有机会同时处理不同任务。
分析异常被 packaged_task 保存，主线程在 future.get() 时接收。

### 3. exchange 转移一项真实责任

`shutdown` 设置关闭状态、唤醒等待线程，再使用 `std::exchange` 把线程容器取走并置空。
局部容器负责 join；成员容器为空，顺序重复关闭时就没有线程需要再次 join。
这里的“置空”来自传入的新值，不是 exchange 对任意类型都自动清空。

关闭后工作线程仍会取出已经排队的任务，队列空时才退出。
这个流程只提供排空语义；shutdown 不支持多个线程同时调用，也没有取消运行中任务。

### 4. 类的成员也有销毁顺序

析构函数体先调用 shutdown 并等待线程结束，随后队列、条件变量、互斥量等成员才销毁。
如果省略等待，工作线程仍可能访问正在被销毁的 this。

检查点：直接在 submit 后调用 shutdown，还没有 get 任何 future，任务应该如何结束？
对照 `test_executor_drain` 的断言。

## 第六遍：元组打印为什么能处理不同类型

打开 [tuple_tools.h](include/signal_lab/tuple_tools.h)。

`tuple_size` 取得字段数量，`make_index_sequence` 生成 0..N-1。
包展开生成对各个 `get<I>` 的访问，再用数组初始化保证按顺序调用访问器。
访问器是捕获 `first=true` 的泛型 lambda，每个字段调用适合自己类型的 `print_cell`。

空元组时仍然保留数组开头的 0，因此不会生成零长数组。
本项目没有 C++17 的 std::apply 或折叠表达式。

## 动手练习

每次只改一项，先预测结果，再编译和运行自检。答案方向放在下表，建议先盖住最后一列。

| 练习 | 任务与验收条件 | 思考方向 |
|---|---|---|
| A 倍率 | 把 demo.settings 的倍率索引改为 1，预测文件模式三份报告 | mean、RMS、X1 都按 2 倍变化；数量不变 |
| B 按类型读报告 | 在 main 取出 Spectrum，使用 `std::tie` 把 real/imag 赋给两个现有 double | 右侧可用 make_tuple；tie 给左侧变量赋值，临时右值销毁不会使变量的值消失 |
| C 代理与副本 | 把内置演示的 `auto proxy` 改成 `bool proxy`，预测任务的选中数量 | 修改独立 bool 不会更新 Series；编译器还可能提示变量设置后未使用 |
| D 自定义透明比较器 | 为 Catalog 写出带 is_transparent 的比较器，支持 string 与 const char* 两个方向 | 键与键、键与查询值必须使用一致的排序；不能改成裸指针地址排序 |
| E 关闭边界 | 给关闭后提交、空指针提交各加一个用例，解释两个错误为何可能先后不同 | submit 先验证输入，再检查关闭状态；按值参数的所有权已转移 |
| F 新增统计量 | 给 Summary 添加峰值，补统计和输出测试 | 优先扩展统计结构，不必向 tuple 塞入第二个同类型 double |
| G 数量边界 | 验证恰好 4,096 点被接受、4,097 点被拒绝 | 同时覆盖输入文件和直接 make_series 两条入口 |
| H 异常恢复 | 提交一个全未选中任务，再提交正常任务 | 首个 future 抛异常，后续任务仍能完成；工作线程不应因此终止 |

进阶设计题：如果新增“取消尚未执行任务”，它与当前的排空关闭有什么区别？
需要如何处理已返回的 futures？先写清规则，再动实现；不要把 C++20 的 stop_token
或 C++23 的 move_only_function 引入这个 C++14 项目。

完成阅读的自查：能不看注释解释每一次 move、每一次持锁、每一次借用以及每一次对象销毁。
返回 [项目说明与知识点对照](README.md)。
