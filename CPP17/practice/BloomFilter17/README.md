# BloomFilter17：可复用的 C++17 布隆过滤器

这是一个使用 C++17 标准库实现的普通 Bloom filter。它用于缓存、SSTable、爬虫和
去重管道的前置筛选：**返回 false 表示一定不存在，返回 true 表示可能存在，需要继续查真实数据。**
这个约定要求过滤器已经包含对应数据集的所有键，并且访问遵守下文的并发规则。

核心接口位于 [bloom_filter.hpp](include/bloom17/bloom_filter.hpp)，
参数规划、统计和快照实现在 [bloom_filter.cpp](src/bloom_filter.cpp)。
工程没有第三方依赖，热路径不分配内存，也不复制传入的字符串。

## 1. 已实现的功能

| 功能 | 接口与行为 |
|---|---|
| 容量规划 | 根据预计不同键数量、目标误判率、内存预算计算位数与探测次数 |
| 字符串与二进制键 | `insert` / `possibly_contains` 接收 `string_view`，支持空键、UTF-8、内嵌零字节 |
| 64 位整数键 | `insert_u64` / `possibly_contains_u64` 使用固定的 8 字节小端编码 |
| 批量操作 | `insert_range` / `count_maybe` 支持单遍迭代器，编译期区分字符串与整数 |
| 清空与复用 | `clear` 保留分配好的空间，可直接重新构建 |
| 并集 | `merge_from` 按位 OR，检查两侧布局和 seed 的兼容性 |
| 交集筛选 | `intersect_with` 按位 AND，保留真实交集的所有键，仍然允许误判 |
| 统计 | 位密度、位数组字节数、误判率估计、元素数估计、指定插入量的理论误判率 |
| 持久化 | 文件与流两种接口，固定字节序、格式版本、哈希版本、CRC32 校验 |
| 值语义 | 深拷贝、强异常安全的拷贝赋值、移动与 swap |

普通 Bloom filter 无法安全删除单个键，也不能在不重新读取原始键的情况下扩容。
超出预计容量后仍然可以插入，不会因为容量超标丢失已插入键，但误判率会升高。
需要删除时应另外设计 Counting Bloom Filter；需要扩容时应重新构建或设计分层过滤器。

## 2. 编译与运行

在仓库根目录执行，使用 Release 构建测量性能：

```powershell
cmake -S CPP17/practice/BloomFilter17 -B CPP17/practice/BloomFilter17/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build CPP17/practice/BloomFilter17/build -j 4
ctest --test-dir CPP17/practice/BloomFilter17/build --output-on-failure

.\CPP17\practice\BloomFilter17\build\bloom_demo.exe
.\CPP17\practice\BloomFilter17\build\bloom_bench.exe
```

Linux / WSL 可使用相同 CMake 工程：

```bash
cmake -S CPP17/practice/BloomFilter17 -B CPP17/practice/BloomFilter17/build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build CPP17/practice/BloomFilter17/build-linux -j 4
ctest --test-dir CPP17/practice/BloomFilter17/build-linux --output-on-failure
./CPP17/practice/BloomFilter17/build-linux/bloom_bench
```

可选项：`BLOOM17_BUILD_BENCHMARK=OFF` 关闭基准程序，`BUILD_TESTING=OFF` 关闭测试。
在 GCC / Clang 环境中，`BLOOM17_SANITIZERS=ON` 开启 ASan / UBSan，适合 Debug 检查。
性能基准请关闭 sanitizer。

## 3. 在自己的程序中使用

```cpp
#include "bloom17/bloom_filter.hpp"

#include <iostream>
#include <string>
#include <vector>

int main() {
    bloom17::BloomFilter filter(100000, 0.01); // 预计十万个不同键，目标误判率 1%
    const std::vector<std::string> keys{"user/1001", "user/1002", "user/1003"};

    filter.insert_range(keys.begin(), keys.end());

    if (!filter.possibly_contains("user/9999")) {
        std::cout << "definitely absent\n";
    } else {
        std::cout << "check the exact data source\n";
    }

    filter.save(std::filesystem::path("users.bloom"));
    const auto restored = bloom17::BloomFilter::load(std::filesystem::path("users.bloom"));
    return restored.possibly_contains("user/1001") ? 0 : 1;
}
```

如果另一个工程通过 CMake 集成：

```cmake
add_subdirectory(path/to/BloomFilter17)
target_link_libraries(your_app PRIVATE bloom17::bloom17)
```

也可以直接编译自己的 `main.cpp` 和 `src/bloom_filter.cpp`，并为编译器添加本项目的
`include` 搜索目录。热路径定义在头文件，链接库负责非热路径函数。

自定义 seed 和内存预算时使用 `Options`：

```cpp
const bloom17::Options options{
    1000000,        // expected_items
    0.01,           // false_positive_rate
    42,             // seed，保存快照时自动保留
    16 * 1024 * 1024 // max_bytes，只约束位数组大小
};

const auto shape = bloom17::BloomFilter::plan(options); // 不分配位数组
bloom17::BloomFilter filter(options);
filter.insert_u64(123456789);
```

整数范围元素会转为 `uint64_t`，有符号负数采用标准无符号转换规则。
整数键与其对应的 8 字节小端二进制字符串使用同一编码；如果业务需要区分类型，
应自行加入类型前缀。字符串按原始字节比较，不自动进行 Unicode 规范化。

## 4. 参数与性能设计

设预计不同键数量为 `n`、目标误判率为 `p`、位数为 `m`、探测次数为 `k`：

```text
理想位数：m ≈ -n × ln(p) / ln(2)²
误判估计：p ≈ (1 - exp(-k × n / m))^k
```

实现先把位数向上取为 2 的幂，最小为 64 位，再寻找满足近似目标的最少探测次数。
`k` 限制为 1～32；找不到合适的 `k` 时继续增加位数，超出内存预算则抛异常。
例如 `n=200000, p=0.01` 时使用 2,097,152 位、256 KiB、5 次探测。

这个选择用额外空间换取更少的随机访存。通常取整后的空间不足理想位数的两倍；
最小 64 位以及极低误判目标下的 32 次探测上限可能带来额外开销。
给定 `p` 是参数规划目标，**不构成任意键分布下的严格误判上界**。

每个键通过 MurmurHash3_x64_128 得到两个 64 位结果，之后按下面的规则定位：

```text
step  = h2 | 1
bit_i = (h1 + i × step) & (m - 1)
```

一次 128 位哈希代替重复计算 `k` 个完整哈希；掩码代替取模。
`m` 是 2 的幂，奇数步长与 `m` 互质，避免退化成只遍历一个很小的位子集。
64 位无符号溢出是这里刻意使用的模运算。

位数组使用连续的 `vector<uint64_t>`。查询遇到第一个零位直接返回，不额外维护
每次插入计数或置位计数；这些工作会增加写路径成本，而且重复插入数不等于不同键数。
这是一种普通全局位数组布局，大过滤器的随机访存仍可能受缓存未命中影响。

## 5. 合并、统计和并发

```cpp
bloom17::BloomFilter left(100000, 0.01, 42);
bloom17::BloomFilter right(100000, 0.01, 42);

left.insert("left");
right.insert("right");
left.merge_from(right);

const auto stats = left.statistics();
const auto theoretical = left.false_positive_rate_for(200000);
```

合并要求实际位数、探测次数、seed 相同。不同构造参数经过取整后可能得到相同布局，
这种情况允许合并；相同容量但 seed 不同则拒绝。合并总数据量增长后也会增加误判率。
`intersect_with` 只构造交集的近似筛选器，不能用于判断两个原始集合是否完全相等。

`statistics()` 扫描全部位数组，复杂度为 `O(m / 64)`，不要放在每次查询之后。
它用位密度 `q` 估计 `q^k`，并反推 `-m/k × ln(1-q)` 作为元素数估计。
完全饱和时元素数返回 `nullopt`；未插入元素时为 `0`。
这些值不是精确去重统计，AND 之后的估计尤其不能解释为真实交集大小。

多个线程可以并发查询已经构建完成、保持不变的过滤器。
插入、clear、合并、赋值与其他访问之间需要外部同步；底层位操作没有使用 atomic。
高吞吐构建可让每个线程使用相同参数独立构建一份，完成后统一合并，再发布只读对象。

对可变数据库，应用需要在同一同步协议下更新数据与过滤器。
尚未追平新增数据的过滤器不能用于否定最近写入。持久化时也需要将快照与对应的数据集版本绑定。

移动后的源对象可以析构、赋值、swap、clear、查询和读取统计；查询返回 false。
对它执行插入、合并或保存会抛异常，重新赋值后即可正常使用。

## 6. 快照格式与错误处理

快照格式固定为：

| 偏移 | 字段 |
|---|---|
| 0～7 | 8 字节 magic：`BLM17\r\n\x1a` |
| 8～11 | 格式版本，当前为 1 |
| 12～15 | 哈希与探测规则编号，当前为 1 |
| 16～23 | 位数，64 位无符号整数 |
| 24～27 | 探测次数 |
| 28～31 | seed |
| 32～39 | 64 位字的数量 |
| 40 开始 | 小端编码的位数组，每字 8 字节 |
| 最后 4 字节 | 前面全部内容的 IEEE CRC32 |

所有整数字段均为小端序，不写入 C++ 对象的原始内存表示。
加载时先验证版本、布局和预算，再分配数组；截断、损坏和不支持的版本都会报错。
文件接口还拒绝末尾多余字节；流接口恰好消费一帧，因此可以拼接快照。
默认加载内存预算为 256 MiB，可以用 `load(input, max_bytes)` 调整。

参数错误抛 `invalid_argument`，容量预算错误抛 `length_error`，
移动后非法操作抛 `logic_error`，快照错误抛 `runtime_error`。
内存分配及启用异常掩码的自定义流还可能抛出标准库自己的异常。

文件保存会覆盖目标文件，并检查写入和关闭错误；它没有实现原子替换或断电持久性。
需要保留旧快照时，应在应用层保存到临时文件、同步后再切换。
加载失败时可以禁用过滤优化并查询真实数据源，不能用空过滤器替代损坏索引继续否定查询。
CRC32 用于检测意外损坏；哈希和 CRC 都不提供对抗恶意输入的安全保证。

## 7. 测试与实测

12 组自检覆盖参数边界、哈希已知向量、所有短键尾部长度、二进制键、批量单遍迭代器、
十万个整数无假阴性、三个 seed 的误判率采样、并集/交集、兼容性、快照往返、
快照逐字节破坏与逐位置截断、文件错误、值语义、饱和状态、并发只读。
断言不会因 Release 下的 `NDEBUG` 而被关闭。

已验证 Windows / MinGW GCC 15.2.0 和 WSL Ubuntu / GCC 13.3.0。
两边均开启严格警告；WSL Debug 同时运行 ASan / UBSan。
另与上游 MurmurHash3 实现对照 0～512 字节、4 个 seed，共 2052 组输入，全部一致。
两平台生成的同数据集快照逐字节一致，CRC32 另外通过 Python `zlib` 对照。

SSTable 演示使用有序数组模拟精确索引，20,000 个键、100,000 次查询的结果为：

```text
filter_bytes=32768 probes=3
backend_lookups=20678 skipped=79322
exact_hits=20000 false_negatives=0
false_positives=678 measured_fpr=0.0085
```

本机 AMD Ryzen 9 8940HX、Windows / GCC 15.2.0、CMake Release 的一次测量：
20 万个不同键、每轮 50 万次查询、预热 1 轮、正式 5 轮取中位数。

| 操作 | 吞吐量 |
|---|---:|
| 整数插入 | 约 168 Mops/s |
| 整数命中查询 | 约 187 Mops/s |
| 整数未命中查询 | 约 114 Mops/s |
| 整数混合查询，50% 命中 | 约 141 Mops/s |
| 约 30 字节字符串插入 | 约 41 Mops/s |
| 约 30 字节字符串未命中查询 | 约 28 Mops/s |

位数组为 256 KiB，负查询采样的误判率为 0.7564%。基准不包含键生成与初始化，
结果取决于 CPU、缓存、编译器、键长度、数据规模和查询分布，不是性能承诺。

基准还包含 `unordered_set` 直接查询和 Bloom + 精确查询。
在本次简单整数、全内存数据集上，`unordered_set` 的负查询更快，增加过滤层反而变慢。
Bloom filter 的主要价值是节省空间，并在真实后端查询成本足够高时减少后端访问。

```powershell
.\CPP17\practice\BloomFilter17\build\bloom_bench.exe --items 1000000 --queries 1000000 --rounds 5
```

## 8. 阅读顺序与 C++17 对应

建议从 `examples/sstable_demo.cpp` 进入，再看头文件中的 `insert_hash` 与 `contains_hash`，
然后阅读 `plan`、`statistics`，最后跟踪快照的保存与恢复。

| C++17 特性 | 在本项目中的作用 |
|---|---|
| `string_view` | 借用字符串和二进制键，不复制输入 |
| `if constexpr` | 批量接口按键类型选择整数或字符串路径 |
| `optional` | 表达饱和后无法可靠估计元素数 |
| `filesystem` | 跨平台文件路径与快照接口 |
| `inline constexpr` | 公共默认参数和编译期 CRC 表 |
| 结构化绑定 / CTAD | 参数解析结果、测试列表和测试分发 |
| `from_chars` | 无区域设置依赖的基准参数解析 |
| `[[nodiscard]]` | 提醒调用者处理查询、规划与加载结果 |

项目按实际需要使用 C++17，没有为了覆盖全部课程而引入额外类型擦除或运行时分发。

## 参考

- [Kirsch / Mitzenmacher：Less Hashing, Same Performance](https://www.eecs.harvard.edu/~michaelm/postscripts/rsa2008.pdf)：双重哈希和误判概率分析。
- [Austin Appleby：MurmurHash3 原始实现](https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp)：public domain；本项目改用显式小端读取和可移植的尾部处理。

返回 [C++17 练习目录](../README.md)。
