# 第 04 课 · std::make_unique 与智能指针最佳实践（C++14）

> 所需标准：C++14 ｜ 讲解：本文件 ｜ 独立代码：`04_make_unique.cpp`

## 一、底层原理

### 1.1 迟到两年的工具：C++11 漏掉了 make_unique

C++11 有了 `make_shared`，却漏了 `make_unique`（Herb Sutter 称之为
"the one that got away"），C++14 补上。它**没有任何编译器魔法**，就是十几行库代码：

```cpp
template <class T, class... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
```

本质一句话：**把 `new` 关进工厂函数内部**。

### 1.2 它到底堵住什么：C++11 的异常安全缺口

```cpp
f(std::unique_ptr<Widget>(new Widget), foo());   // C++11 下有泄漏风险！
```

| 求值步骤 | C++11 允许的坏顺序 |
|---|---|
| 1 | `new Widget` 先执行 |
| 2 | `foo()` 执行并抛异常 |
| 3 | `unique_ptr` 构造函数**还没来得及跑** → 裸指针无人接管 → **泄漏** |

C++11 只保证实参之间"非全序"，每个实参内部的操作可以任意交错。
而 `make_unique<Widget>()` 把 `new` 关在自己的函数帧里——它和 `foo()` 之间
只能整体排队，交错消失，泄漏不可能发生。

**诚实备注**：C++17（P0145）规定实参之间不得交错，语言层面已堵住这个缺口。
但 make_unique 仍是首选——不拼 `new`、类型只写一遍、意图清晰（见 3.3）。

### 1.3 unique_ptr 本体为什么零开销

- **删除器是类型的一部分**：默认删除器是空类，被"空基类优化"（EBO）吃掉
- 主流实现 `sizeof(unique_ptr<T>) == sizeof(T*)`——独占指针就是裸指针的价格
- **拷贝构造被 `=delete`**：所有权是编译期概念（move-only），
  不是引用计数那种运行期机制

| | unique_ptr | shared_ptr |
|---|---|---|
| 对象大小 | 一个指针 | 两个指针 + 控制块 |
| 运行期开销 | 零 | 原子引用计数 |
| 所有权模型 | 独占，move 转移 | 共享，拷贝 +1 |
| 工厂函数的额外好处 | 类型写一遍、异常安全 | 上述 + 单次分配（控制块合并） |

## 二、具体语法

### 2.1 三种形式

```cpp
auto p  = std::make_unique<Widget>(args...);   // 单对象，完美转发
auto pa = std::make_unique<Widget[]>(n);       // 数组，值初始化为 0
// std::make_unique<Widget[10]>                // 编译错误：有意禁止定长数组
```

### 2.2 规则细则

- 不支持**自定义删除器**——需要时直接构造 `unique_ptr<T, Deleter>`
- `unique_ptr` 可隐式转换成 `shared_ptr`；反向不存在
- `release()` 是唯一的"裸奔口子"（交出所有权，调用方接管）
- 容器装独占指针：`push_back(std::make_unique<T>(...))`，无需显式 `move`
  （C++11 需要写 `std::move`，因为实参是左值）

## 三、实际应用与最佳实践

### 3.1 场景：工厂函数的默认返回类型

```cpp
std::unique_ptr<Widget> make_widget(int v) {
    return std::make_unique<Widget>(v);
}
```

调用方想要独占就拿 `auto`，想要共享就让它隐式升级成 `shared_ptr`——
工厂不替客户做所有权决定。现代工厂的默认契约。

### 3.2 场景：容器接管一批对象的所有权

`std::vector<std::unique_ptr<Widget>>` 让容器成为唯一拥有者：
自动析构、不可意外拷贝、需要时还能 `move` 出来。多态场景配虚析构即可。

**析构顺序的两个规则**（示例的结尾可以亲眼验证）：
自动变量按**声明逆序**析构（先构造的后死）；`vector` 析构元素按**存储正序**
（v[0] 先于 v[1]）。两种顺序放在一起时别想当然。

### 3.3 最佳实践清单

1. **代码里不出现裸 `new`**：一律 make_unique；唯一合法例外是需要自定义删除器
2. **默认独占**：先 unique_ptr，确需共享再改 shared_ptr——独占是更好的默认契约
3. **函数参数**：只是"用一下"对象时传 `T&` 或 `T*`，不要传智能指针
   （所有权语义属于类成员/返回值，不属于借用参数；GotW #91、Core Guidelines R.32）
4. **动态数组**：能 `std::vector` 就别 `make_unique<T[]>`——vector 记得自己的大小
5. `make_shared` 的单次分配优势**不适用于** `make_unique`（unique_ptr 没有控制块）

## 常见陷阱

1. **存下 `get()` 的返回值**：unique_ptr 析构/转移后，存下的裸指针悬空
2. `make_unique<T[]>(n)` 会**值初始化写零**——大数组的热路径注意；
   只想拿内存用 C++20 的 `make_unique_for_overwrite`
3. 自定义删除器必须**直接构造** unique_ptr，且删除器类型进入类型系统
   （不同删除器 = 不同类型），sizeof 可能变大
4. 用 `unique_ptr<Base>` 持有多态对象时，**Base 析构函数必须 virtual**，
   否则 delete 走不到派生类析构（行为未定义）
5. `release()` 与 `reset()` 混淆：release 交出所有权返回裸指针（调用方接管），
   reset 立即析构旧对象——记反了就是泄漏或双重释放
6. 析构顺序想当然：自动变量是**声明逆序**，`vector` 元素是**存储正序**——
   多对象依赖析构顺序的代码（如日志单例）要额外小心

## 完整示例

见 `04_make_unique.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织。
①里用构造/析构打印亲眼看"异常路径下 unique_ptr 兜底 vs 裸 new 泄漏"的对比：

```bash
g++ -std=c++14 -Wall -Wextra "CPP14/04_make_unique与智能指针最佳实践/04_make_unique.cpp" -o run && ./run
```

## 练习

1. 用 `unique_ptr` + 自定义删除器管理一个假文件句柄（删除器打印 "file closed"），
   验证离开作用域时被调用。
2. 实现多态家族 `Shape`/`Circle`/`Square`（虚析构），放进
   `vector<unique_ptr<Shape>>`，观察析构顺序；再删掉 `virtual` 看看会发生什么。
3. 写 Pimpl 最小骨架：`class Widget { struct Impl; unique_ptr<Impl> p_; public: Widget(); ~Widget(); };`
   （提示：析构函数必须类外定义，为什么？EffModern Item 22）
4. 给 `make_widget` 加日志，验证"unique_ptr 隐式升级 shared_ptr"只发生一次构造。

## 参考

- GotW #91: Smart Pointer Parameters；C++ Core Guidelines R.32 / R.34
- 《Effective Modern C++》Item 18–22（重点 Item 21）
- cppreference: [std::make_unique](https://zh.cppreference.com/w/cpp/memory/unique_ptr/make_unique)
