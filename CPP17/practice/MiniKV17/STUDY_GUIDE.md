# MiniKV17 阅读与动手指南

建议分 6 次阅读，每次先预测行为，再沿调用链验证。先理解成功路径，第二遍再追踪恢复和异常。
核心实现约 1500 行，另附约 500 行测试。代码按逻辑阶段留白，
可以在纸上记录每次操作后的“内存表 / 日志 / 指标”。

## 第一次：从一次 put 走到磁盘

打开 [src/main.cpp](src/main.cpp)，跟随：

```text
main
  → Store::open
  → Shell::run
  → parse_command
  → Shell::execute
  → Store::put
  → Store::commit
  → Journal::append
```

执行 `put jobs.done int 12`，观察这几种表示如何变化：

1. 输入最初是一行有所有权的 `string`。
2. 解析器借用 `string_view` 扫描，把结果放入拥有字符串的 `Put`。
3. `Command` 是固定命令集合的 `variant`，`visit` 选择 `Put` 分支。
4. 参数被移动到 CTAD 推导的 `tuple`，`apply` 调用 Store 的成员函数。
5. 内存条目将“有没有过期时间”表示为 `optional`，值类型仍是 `variant`。
6. 提交前准备 map 节点；日志成功后才把节点转入正式表。

检查点：如果把原始输入字符串清空，已解析的 Put 是否仍然有效？
如果把 `Store::open` 改成返回一个命名局部变量，为什么不可移动的 Store 可能无法编译？
本项目依赖的是返回 `Store(...)` 纯右值的强制消除，不是可选 NRVO。

## 第二次：决定什么时候使用 optional / variant / any

打开 [include/minikv/model.hpp](include/minikv/model.hpp) 和
[include/minikv/shell.hpp](include/minikv/shell.hpp)，对照：

| 问题 | 表示 | 原因 |
|---|---|---|
| 一个键有没有值？ | `optional<Entry>` | 查询不到属于正常业务状态 |
| 值是字符串、整数还是布尔？ | `variant<string, int64_t, bool>` | 候选类型固定，需要类型安全访问 |
| 一次写入是成功还是有错误？ | `variant<Ok, Error>` | 失败必须携带原因，不能只有空值 |
| 宿主想附加哪种审计结构？ | `any` | 核心 Shell 不预先枚举外部扩展类型 |

`main` 注册 `SessionAudit`，但 `Shell` 只保存 `any` 和观察器，不依赖该具体结构。
将观察器里的 `any_cast<SessionAudit>` 改为 `any_cast<string>`，指针版应返回空指针。
如果换成引用版，就需要处理 `bad_any_cast`。

动手：定义自己的 `struct Audit { size_t ok, failed; };`，替换 main 中的观察器。
不要把核心 Value 改成 any，否则类型约束、序列化和错误检查都会变成运行期约定。

## 第三次：时间、锁与快照

重点阅读 [src/store.cpp](src/store.cpp) 的 `get`、`increment`、`stats`。

`get` 在共享锁下复制 Entry，释放数据锁后再单独更新读指标。
`increment` 将读、检查溢出、写日志、更新值放在同一个独占临界区中；
若在调用方用 `get → +1 → put` 拼接，多线程之间就会丢失更新。

数据由 `shared_mutex` 保护，指标由独立 `mutex` 保护。
写入和统计快照需要同时访问两者，因此使用 `scoped_lock`。
不要在持有指标锁时手工等待数据锁；观察器则在这些锁全部释放后调用。

TTL 的规则是 `expires <= now` 时不可见。写入相对 TTL，日志保存绝对截止时间。
例如在时间 100 写入 TTL=5，时间 104 重启，过期时间仍是 105。
读取不会启动后台删除；写入与压缩会清理已过期数据。

检查点：`scan` 返回后再执行一次 put，之前得到的 Row 是否变化？
`get` 后立刻调用 `stats`，二者为什么不能自动视为一个事务？
读统计和扫描统计各自有自己的观察时刻，不应在并发测试中强行断言它们逐字段对应。

`stats` 在两个锁内采集长度和指标，锁外归约长度快照。
默认 seq 的结果应与 par 一样；par 的回调不能更新共享计数器，也不能在其中调用 Store。
本项目最多 10000 个键、每个值最多 65536 字节，长度求和不会发生 uint64_t 溢出。

## 第四次：亲自解读一条日志

打开 [src/journal.cpp](src/journal.cpp)。日志是一系列 frame，所有整数都显式使用小端编码：

| 字段 | 字节数 | 说明 |
|---|---|---|
| magic | 4 | ASCII `KV17` |
| payload_size | 4 | 后续载荷的字节数，读取前检查上限 |
| payload_checksum | 4 | 载荷的 FNV-1a 校验值 |
| header_checksum | 4 | 前 12 字节的 FNV-1a 校验值 |
| payload | 可变 | 一条 Write / Erase / Rename |

payload 首字节为操作号：1=Write、2=Erase、3=Rename。
文本统一使用 `u32 长度 + 原始字节`，不会把结构体内存布局写到文件。

- Write：操作号、键、`u64 expires`、值类型号、值文本。expires=0 表示永久有效。
- Erase：操作号、键。
- Rename：操作号、源键、目标键。

值类型号为 0=字符串、1=int64、2=bool。整数通过 charconv 转十进制文本，布尔为 true/false；
字符串可包含零字节。`std::byte` 保存原始字节，转成数值时使用 `to_integer`。
`reinterpret_cast` 仅在流的字节输入输出边界使用，不用它恢复 C++ 对象。

检查点：为什么长度字段还需要头校验？
若损坏把长度改大，只校验载荷可能把损坏记录误认为尚未写完的尾部；头校验可提前拒绝。
半个头或头合法但载荷不完整仍可作为尾记录截断；校验无法提供密码学防篡改保证。

整数解析必须同时检查错误码与结束指针。`from_chars("12tail")` 的前半段成功，
不代表整个字段合法；`parse_command` 和 `decode` 都需要坚持完整消费。

## 第五次：把失败路径画出来

写入顺序：

```text
验证参数、TTL、容量
  → 在临时 map 中分配节点
  → 编码整条 frame
  → append + flush
  → 将节点转入正式 map
  → 更新 writes
```

因此参数失败不追加日志；编码或节点分配失败发生在文件写入前。
真正的文件写入失败可能留下部分 frame，此时禁止继续追加。
重新打开后，完整有效记录会重放，未完成尾部会被截断；此前失败命令的结果可能不确定。

压缩时，当前有效行被写入 `store.next`，随后执行：

```text
关闭旧输出流
  → store.wal 重命名为 store.backup
  → store.next 重命名为 store.wal
  → 重新打开追加流
  → 尝试删除 store.backup
```

| 重新打开时的现场 | 处理 |
|---|---|
| wal 存在，只有 next 是残留 | 校验并使用 wal，删除 next |
| wal 不存在，backup 存在 | 把 backup 恢复为 wal，忽略 next |
| wal 和 backup 都存在 | 校验并使用 wal；成功后清理 backup |
| wal 存在但完整记录校验失败 | 明确失败，不自动使用旧 backup 隐藏数据损坏 |

以上讨论限定在文件操作可见的现场，不承诺断电后的写入顺序。
扩展为生产级持久化之前，需要加入平台文件同步、目录同步、跨进程排他和故障注入。

## 第六次：带着问题看测试

[tests/self_test.cpp](tests/self_test.cpp) 中的测试逐组建立独立目录，读取预测如下：

| 测试 | 需要验证的性质 |
|---|---|
| parsing | 输入完整、数值边界、Command 不悬垂 |
| persistence | 三类值和变更跨重启保持一致，整数极值不损失 |
| ttl | 到期边界精确，重命名和重启不续期 |
| validation_and_snapshots | 失败不追加，副本不借用内部内存 |
| compaction / empty_store | 历史被替换后还能追加、重开，空日志也合法 |
| torn_tail / corruption | 不完整尾部可修复，完整损坏会拒绝 |
| interrupted_switch | 构造不同文件现场，选择正确的提交依据 |
| compaction_failure | 压缩失败后的状态与写入屏障 |
| atomic_counter | 并发增量不丢失，溢出/类型错误不写入，重放不重复增量 |
| capacity | 满容量拒绝新键，仍可覆盖、删除与补入 |
| concurrent_access | 并发读写、统计快照和最终更新一致 |
| shell_session | 命令分发、顺序输出、转义、quit 和 any 观察器 |

最后做三项练习，每项先写预期再实现：

1. 增加 `exists KEY`：明确它是否计入 reads，再扩展 Command 和访问器。
2. 增加 compare-and-set：把比较与提交放在同一个临界区中，测试竞争下只有一个写入者成功。
3. 将恢复改成流式回调：避免暂存全部 Mutation，保持损坏时的失败语义与尾部修复行为。

完成后，尝试只看公共 Store 接口写一个小型本地配置工具；这能验证模块边界是否易于复用。
