# 第 30 课 · 标准库修补合辑：std::byte、map 节点句柄与 try_emplace（C++17）

> 所需标准：**C++17** ｜ 讲解：本文件 ｜ 独立代码：`30_byte_node_try_emplace.cpp`
> —— CPP17 篇收官课；下一课起进入 C++20（`-std=c++20`）

## 一、底层原理

### 1.1 std::byte：给"原始内存"一个专属类型

表示字节的旧选择——`char`/`unsigned char`/`uint8_t`——**全都是整数**：
可以 `+1`、比大小、参与算术，于是"把缓冲区当数字算"的语义错误畅通无阻。
`std::byte` 是独立的枚举类型，只保留**位运算**（`<< >> | & ^ ~`），
算术一律编译错；与整数的互转必须显式 `std::to_integer<T>`。
**类型即文档**："这是字节，不是数字"。

### 1.2 节点句柄：map 元素的"零拷贝搬家"

map 的 key 是 const——元素**移动**都做不到，C++17 前"搬家"只能拷贝。
P0083 给底层红黑树开了个口：`extract()` 把整个**节点**从树里拔出，
所有权交给 node handle（RAII：析构时仍持有则销毁节点）；
`insert(handle)` 把节点插回任意同容器（无拷贝无再平衡分配）。
map 的 handle 还提供 `key()` —— **唯一能修改 map 元素 key 的入口**。

### 1.3 try_emplace / insert_or_assign：修补 operator[] 与 emplace 的两宗罪

| 罪状 | 旧写法的行为 | C++17 修补 |
|---|---|---|
| `m.emplace(k, std::move(v))` 键冲突 | pair 节点已构造 → **v 已被搬空**再丢弃 | `try_emplace(k, std::move(v))`：键存在则**参数分毫不动** |
| `m[k] = v` 查找/写入二合一 | 不存在时**先默认构造再赋值**（要求可默认构造）；纯查找也会**悄悄插入** | `insert_or_assign(k, v)`：存在覆盖/不存在插入，无默认构造 |
| 元素跨 map 搬家 | 只能拷贝/移动（key 是 const） | `extract` + `insert`（节点级零拷贝） |
| 整表合并 | 循环逐个拷贝 | `m.merge(src)`（冲突节点留在源表） |

`try_emplace` 的"参数保全"性质是重点：**键已存在时实参不会被消耗**
（move 语义根本不发生），拿 unique_ptr 一试便知（示例 ②）。

## 二、具体语法

```cpp
std::byte b{0x0F};                  // 只能花括号从整数构造（= 隐式转换不存在）
b |= std::byte{0x10};  b <<= 1;     // 位运算 OK
// b += 1;                          // 编译错：无算术
std::to_integer<int>(b);            // 显式转整数

auto h = m.extract(it_or_key);      // 拔节点：m.size() 减一
h.key() = "renamed";                // map 专属：改 key
auto r = m2.insert(std::move(h));   // 注意返回 insert_return_type，不是 pair！
// r.inserted / r.position / r.node —— 失败时节点从 r.node 收回（h 已变空）
m.try_emplace(k, args...);          // pair<iterator,bool>；冲突时 args 不动
m.insert_or_assign(k, v);           // pair<iterator,bool>；bool=是否新插入
m.merge(src);                       // 冲突节点留在 src
```

规则细则：

- handle 为空时 `key()`/`value()` 是 UB——先查 `h.empty()`（libstdc++ 会断言）
- `insert(std::move(h))` **成功**：handle 变空；**失败**：节点在返回值
  `insert_return_type::node` 里（handle 已被 move-from）——别再从 h 里找
- 带提示的重载 `insert(hint, std::move(h))` 失败时才真正**保留 handle 所有权**
- `merge` 只搬不冲突的；冲突的**留在源表**（不是报错也不是丢弃）
- `std::byte b = 5;` 编译错——`std::byte{5}` 才行（防隐式整数混入）

## 三、实际应用与最佳实践

### 3.1 场景：带昂贵资源值的缓存（示例 ②）

值是 unique_ptr/大字符串/move-only 时，`emplace` 冲突即"资源被搬走"的
暗坑——`try_emplace` 一行修复。规则化：**值含移动语义就一律 try_emplace**。

### 3.2 场景：改 key 与跨容器迁移（示例 ③）

配置改名（key 更新）、分片 map 合并、LRU 淘汰到冷备表——extract/insert
让这些操作从 O(n) 拷贝变成节点级 O(log n) 零拷贝。

### 3.3 最佳实践清单

1. 内存缓冲区接口签名用 `std::byte*`/`span<std::byte>`，不用 char*
2. map 插入：可能冲突且值可移动 → try_emplace；总要写入 → insert_or_assign
3. **纯查找永远不用 operator[]**（会插入！）——find/count（C++20 有 contains）
4. 跨 map 搬元素/整表合并 → extract/merge，忘掉拷贝循环
5. handle 用完即 insert 或让它析构（都是安全路径）；别长期持有

## 常见陷阱

1. `emplace` 键冲突时参数已被消耗（move 掉）——症状是"值莫名变空"，
   换 try_emplace
2. `m[k]` 在 const map 上编译错，在非 const map 上**悄悄插入空值**——
   查找误用 operator[] 是统计类 bug 的常见来源
3. **insert(node_type&&) 失败后找错了地方**：节点在返回值 `.node` 里，
   不在被 move 的 handle 里（本课实测教训：从空 handle 调 `key()` 直接
   断言崩溃）；带 hint 的重载才是"失败保留 handle"
4. `std::byte` 与整数比较/相加全编译错——这是特性；需要数值语义先
   `to_integer` 明示
5. merge 后不检查源表剩余——冲突节点留在源表，"我以为全搬完"是事故起点

## 完整示例

见 `30_byte_node_try_emplace.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++17 -Wall -Wextra "CPP17/30_std_byte节点句柄与try_emplace/30_byte_node_try_emplace.cpp" -o run && ./run
```

## 练习

1. 用 `vector<std::byte>` 写 hex dump：按 16 字节一行打印十六进制与
   可见 ASCII（to_integer 显式转出来）。
2. 验证 extract 后 map 的迭代器/size 状态；故意对空 handle 调 `key()`
   编译能过吗？（UB 陷阱：加 `assert(!h.empty())` 防御）
3. 写"配置改名"工具：extract → 改 key → insert 回原 map，对比直接
   `m["new"] = m["old"]; m.erase("old");` 的拷贝成本（用 Traced 计数）。
4. 用 try_emplace 实现"首次出现记序号"：`try_emplace(w, next_id++)`
   —— 冲突时 next_id 不增长，体会参数保全的妙用。

## 参考

- 提案 P0298R3（std::byte）/ P0083R3（节点句柄与 merge）/ N4279（try_emplace）
- cppreference: [std::byte](https://zh.cppreference.com/w/cpp/types/byte)
- cppreference: [map::extract](https://zh.cppreference.com/w/cpp/container/map/extract)
- cppreference: [map::try_emplace](https://zh.cppreference.com/w/cpp/container/map/try_emplace)
