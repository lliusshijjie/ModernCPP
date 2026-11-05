# 第 18 课 · std::any（C++17）

> 所需标准 **C++17** ｜ 头文件 `<any>` ｜ [独立示例](18_std_any.cpp)

扩展组件可以各自定义配置项。公共存储层只需要知道键名和如何保存一个值，读取端则知道自己要哪种类型。这节课用 `std::any` 实现这个交接过程。

## 一、底层原理

### 1.1 用统一的外壳保存不同的类型

`std::any` 为空，或者持有一个具体对象。构造时，模板从传入值推导出类型；构造完毕后，变量对外统一表现为 `std::any`。外部代码无需知道内部类型就能复制或销毁容器。读取者通过 `std::any_cast<T>` 提供预期类型，库在运行期检查匹配后才交出值。

这就是**类型擦除**。可以把实现想象成“存储空间 + 一组按实际类型生成的操作”，其中包含复制、销毁和查询类型所需的信息。小对象可能直接存放在容器内部；具体大小和对齐条件由标准库决定，且标准将此优化限定于可无异常移动构造的类型。工程代码应把分配行为当作实现细节。

### 1.2 值语义与类型衰减

普通值构造保存的是 `std::decay_t<T>`，且被存类型必须可复制构造。传入 `const int` 左值时，`any` 自己持有一个 `int`。复制整个 `any` 会复制其中的对象，所以示例 ① 将原容器中的值改为 `9` 后，源变量和副本仍为 `7`。具体的复制行为由对象自己的复制构造决定，比如复制 `shared_ptr` 后仍共享同一个目标对象。

数组会退化为指针，直接放入字符串字面量保存的是 `const char*`。需要容器持有字符串内容时，显式传入 `std::string`。

## 二、具体语法

### 2.1 查看、读取和修改

以下操作以 `std::any value = 7;` 为起点。

| 写法 | 行为 |
|---|---|
| `value.has_value()` | 检查是否持有对象。 |
| `value.type() == typeid(int)` | 判断实际类型。空容器的 `type()` 返回 `typeid(void)`。 |
| `std::any_cast<int>(value)` | 从左值容器取出一个值，此处复制为一个新的 `int`。 |
| `std::any_cast<const int&>(value)` | 只读访问内部对象。 |
| `std::any_cast<int&>(value) = 9` | 修改容器中的对象。 |
| `std::any_cast<int>(&value)` | 匹配时返回内部对象的指针，失败时返回 `nullptr`。传入 `const std::any*` 得到 `const int*`。 |

匹配依据是实际存储类型。去掉请求类型的引用与顶层 `const` / `volatile` 后，`int` 仍然只与 `int` 匹配。`any_cast<double>(value)` 抛出 `std::bad_any_cast`，`any_cast<double>(&value)` 返回 `nullptr`。数值转换需要先按实际类型读出，再显式转换。

如需移出内部对象，可使用 `std::any_cast<T>(std::move(value))`。它以内部对象作为右值来构造结果。成功后容器仍持有一个 `T`，其值取决于 `T` 的构造行为。如果要让容器变空，调用 `reset()`。

### 2.2 构造、替换和清空

```cpp
std::any empty;
std::any text{std::in_place_type<std::string>, 3, 'a'}; // "aaa"
auto numbers = std::make_any<std::vector<int>>({1, 2, 3});
auto& current = text.emplace<std::string>("ready");    // 引用指向新对象
current += "!";
text.swap(numbers);  // 交换两个容器的状态
numbers.reset();     // 销毁交换后持有的字符串，容器变空
```

原位构造把参数直接交给被存类型的构造函数。`emplace` 先销毁旧值再构造新值，因此新值构造抛异常时容器变空。示例 ② 用同一个会在复制时抛异常的类型，分别走两条路径。复制赋值在准备新值失败后保留原来的 `42`，`emplace` 在构造失败后留下空容器。

## 三、实际应用与最佳实践

### 3.1 场景 · 扩展组件的配置

示例 ③ 用 `std::map<std::string, std::any>` 保存各扩展组件写入的选项。`printIntOption` 在读取边界先用 `find` 区分键是否存在，再用指针版 `any_cast<int>` 检查类型。

| 请求的键 | 实际状态 | 输出 |
|---|---|---|
| `retry_count` | 存在，值为 `int{3}` | `retry_count: 3` |
| `timeout_ms` | 存在，值为 `double{250.0}` | `timeout_ms: expected int` |
| `buffer_size` | 缺键 | `buffer_size: missing` |

`output_dir` 展示同一张表可以保存字符串。实际工程中，读取方可先把自己负责的字段校验并整理成明确类型的配置结构，再传给业务代码。若允许把空 `any` 写入表中，`find` 仍能找到这个键，读端可用 `has_value()` 为它定义独立状态。

### 3.2 最佳实践

- 让组件在注册或读取处声明每个键对应的预期类型。类型不匹配属于常见输入时，用指针版 `any_cast` 并显式处理失败。
- 使用 `any_cast<const T&>` 或指针读大对象，避免额外复制。借用只持续到内部对象被销毁或替换前。
- 类型集合固定且需要逐种处理时，沿用第 17 课的 `std::variant`。值类型固定但可能缺失时，用第 16 课的 `std::optional<T>`。

## 常见陷阱

1. **字符串字面量的类型**。直接存入 `"ready"` 后，应按 `const char*` 取值；`std::any_cast<std::string>` 需要容器实际持有字符串对象。
2. **借用的生命周期**。指针和引用都依赖被存对象。调用 `reset()` 或 `emplace()` 后应重新取值；借助 `std::ref(x)` 存入的是 `std::reference_wrapper<T>`，`x` 仍需由调用者保持存活。
3. **只能移动的类型**。`std::unique_ptr<T>` 无法直接作为被存值，因为复制 `any` 时需要复制其内容。
4. **用下标读取配置**。对可写的 `map` 使用 `options[key]` 会给缺失的键插入空 `any`，导致一次查询改变配置。示例使用 `find` 保留缺键信息。

## 完整示例

从仓库根目录运行。先预测原值、副本、两次异常后的状态以及三个配置键的结果，再对照代码中的输出注释。

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic -pedantic-errors CPP17/18_std_any/18_std_any.cpp -o run.exe
.\run.exe
```

## 练习

1. 把 `timeout_ms` 从 `250.0` 改成 `250`，预测输出。然后改成 `250.5`，让读端显式接受 `double` 并保留小数。
2. 给配置表添加 `{"buffer_size", std::any{}}`，把读端改成分别输出 `missing`、`empty` 和 `expected int`。用不同类型的值覆盖这三条路径。
3. 存入 `std::ref(x)`，通过 `std::any_cast<std::reference_wrapper<int>>` 修改 `x`。对照示例 ① 的按值复制，观察哪一种会改变源变量。使用 `std::ref` 时包含 `<functional>`。
4. 将 `std::vector<int>` 存入容器，再通过 `std::any_cast<std::vector<int>>(std::move(value))` 移出。验证返回值和 `value.has_value()`，再调用 `reset()` 验证空态。

## 参考

- [C++17 工作草案 N4659 · `[any]`](https://timsong-cpp.github.io/cppwp/n4659/any)
- [WG21 P0220R1 · 将 Library Fundamentals TS 的组件纳入 C++17](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0220r1.html)
