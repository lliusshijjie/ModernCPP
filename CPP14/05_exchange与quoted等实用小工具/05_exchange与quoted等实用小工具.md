# 第 05 课 · std::exchange、std::quoted 等实用小工具（C++14）

> 所需标准：C++14 ｜ 讲解：本文件 ｜ 独立代码：`05_utility_small_tools.cpp`

> 补充阅读：[05A · 透明比较器与异构查找](../补充知识点/05_补充A_透明比较器与异构查找/05_补充A_透明比较器与异构查找.md)，
> 接着阅读 [05B · 非成员迭代器与双范围算法](../补充知识点/05_补充B_非成员迭代器与双范围算法/05_补充B_非成员迭代器与双范围算法.md)。

## 一、底层原理

### 1.1 std::exchange：三行代码的原语

```cpp
template <class T, class U = T>
T exchange(T& obj, U&& new_val) {
    T old = std::move(obj);           // 1. 偷走旧值
    obj  = std::forward<U>(new_val);  // 2. 写入新值
    return old;                       // 3. 返回旧值
}
```

没有魔法，但值得进标准库的原因是：**"偷资源 + 顺手置空源"是移动语义里
最高频、又最容易写错的样板**——手写时忘记把源置空，就是双重释放。
原语化之后这一步无法写错。

**编译器视角**：纯值操作（move-assign + return），返回值靠 NRVO/move，
零运行期开销。

### 1.2 std::quoted：I/O 层的转义协议

`std::quoted` 是生成**流操纵对象**的函数：将该对象插入流或从流提取时，
对当前字段执行包裹与转义，而不是给后续字段设置持久的转义模式：

| 操作 | 行为 |
|---|---|
| `out << quoted(s)` | 包上引号，内部的 `"` 与 `\` 转义为 `\"`、`\\` |
| `in >> quoted(s)` | 吃掉引号，反转义还原 |

核心不变式：**往返安全**——`out << quoted(s)` 再 `in >> quoted(s2)`，
无论 `s` 含空格、引号还是反斜杠，必有 `s2 == s`。
旧写法是每处手拼转义规则，十个模块十种转义，quoted 让协议全工程统一。

### 1.3 _t 别名家族与四迭代器 equal

- C++14 给一批具有 `::type` 的 traits 补了 `_t` 别名：`remove_const_t<T>` ≡
  `typename remove_const<T>::type`——模板代码里的 `typename ... ::type`
  噪音直接减半（配合第 03 课的 `_v` 惯用法，元编程可读性大增）
- `std::equal` / `std::mismatch` 新增**四迭代器重载**：三迭代器老版
  不掌握第二范围的结束位置，要求第二范围足够长；四迭代器版可以检查双方边界。
  `equal` 的双范围版还要求完整长度相等；`mismatch` 则在首个差异或任一端点停止。

## 二、具体语法

### 2.1 exchange

```cpp
int a = 10;
int old = std::exchange(a, 99);     // old == 10, a == 99
```

规则细则：

- 返回**旧值**（按值返回，可 move）；别把参数顺序记反
- 新值类型 `U` 可与 `T` 不同，走转换赋值（`exchange(int_var, 3.14)` 合法）
- 典型落点：构造函数**成员初始化列表**、移动构造/赋值、"取走并替换"

### 2.2 quoted

```cpp
out << std::quoted(s);              // 默认 '"' 包裹、'\' 转义
out << std::quoted(s, '\'');        // 自定义包裹符（如 CSV 里的单引号）
in  >> std::quoted(s);              // 读取时反转义
```

规则细则：只对 `std::string` / 字符数组有意义；**读写必须配对**；
只转义 `"` 和 `\`（换行不转义！）；读到流尾没等到闭引号 → 设置 `failbit`。

### 2.3 _t 别名与四迭代器 equal

```cpp
using raw_t = std::remove_const_t<int* const>;   // int*
bool eq = std::equal(f1, l1, f2, l2);            // 四迭代器：长度也参与比较
```

## 三、实际应用与最佳实践

### 3.1 场景：移动成员的实现（示例的 Buffer）

移动构造两行 exchange 完事；移动赋值"释放旧资源 + 偷新置空"各一行。
对比 C++11 手写版（std::move + 手动 `other.ptr = nullptr`），出错面小一个量级。

### 3.2 场景：字段落盘（配置/日志）

字段里含逗号、引号、空格时，用 quoted 包裹后写入，读取时反转义——
自制 CSV/配置格式从此不怕特殊字符。

### 3.3 最佳实践清单

1. 管理裸资源的移动成员可用 exchange 表达"取走并置空"，同时检查自移动和异常安全
2. 含特殊字符的字段写文件用 quoted，别手拼引号
3. 长度不确定的两范围比较，一律四迭代器 equal（三迭代器版是历史遗留）
4. 模板里用 `_t` 别名降噪（C++17 起配套 `_v`，见第 03 课练习）
5. exchange 后参数对象取得传入的新值；只有传入空状态时才表示"置空"

## 常见陷阱

1. **参数顺序**：`exchange(a, b)` 是"a 换成 b，返回 a 的旧值"，不是交换
   （交换要用 `std::swap`）
2. quoted **不编码换行符**，原始换行会留在输出中；直接配对提取可还原，
   但外层若先按行拆分记录，可能截断字段，应根据文件协议选择写法
3. quoted 读失败会设置流错误状态；默认异常掩码下不抛出流异常，启用相关异常时则可能抛出
4. 三迭代器 `equal` 要求第二范围足够长；它更长时只比较前缀，
   不会仅因两个完整范围长度不同就产生未定义行为
5. 自移动 `b = std::move(b)`：移动赋值若不防 `this == &other`，
   exchange 版也会"自盗自"（先 delete 自己的资源再从自己偷）——加防御判断

## 完整示例

见 `05_utility_small_tools.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++14 -Wall -Wextra "CPP14/05_exchange与quoted等实用小工具/05_utility_small_tools.cpp" -o run && ./run
```

## 练习

1. 给示例的 `Buffer` 加 `swap` 成员函数（用三次 exchange 实现），
   并说明它为什么能标 `noexcept`。
2. 构造含 `"`、`\`、空格、制表符的字符串，用 quoted 做往返测试，
   再加入换行符，对比直接用 quoted 提取与先用 getline 拆记录的区别。
3. 用 exchange 实现 `unique_ptr` 所有权链式转移，转移前后打印两个指针状态。
4. 构造一组"三迭代器 equal 越界读、四迭代器返回 false"的输入，
   用 `-fsanitize=address` 对比验证（选做，体验 UB 检测器）。

## 参考

- cppreference: [std::exchange](https://zh.cppreference.com/w/cpp/utility/exchange)
- cppreference: [std::quoted](https://zh.cppreference.com/w/cpp/io/manip/quoted)
- cppreference: [std::equal](https://zh.cppreference.com/w/cpp/algorithm/equal)
