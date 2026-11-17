# 第 52 课 · deducing this：显式对象形参（C++23）

> 所需标准：**C++23**（`-std=c++23`）｜ 独立代码：`52_deducing_this.cpp`
> 本机示例使用 print，需链接 -lstdc++exp；语言特性自身不需要它。
> 前置：第 41 课（模板 lambda）；第 27 课（值类别）

## 一、底层原理

### 1.1 对象也可以成为显式函数参数

传统成员函数通过隐式对象参数访问对象，函数体内 this 是指向相应 cv 限定类
类型的指针纯右值，不是一个可赋值的 T* const 局部变量。要分别处理对象的
const 与值类别，常写四个合法的重载：

```cpp
void f() &;
void f() const &;
void f() &&;
void f() const &&;
```

C++23 允许把对象写成第一个**函数形参**，并可按需要让其类型参与模板推导：

```cpp
void f(this Widget& self);      // 仅非 const 左值，类似传统 f() &
void g(this auto&& self);       // 推导 cv 与值类别，也可推导派生类型
```

`this Widget&` 本身并不推导所有 cv/ref 形态，`this auto&&` 才是相应的泛型形式。
显式对象形参不一定是模板参数，例如第一个 f 就不是函数模板。

### 1.2 没有隐式 this，但仍是成员函数

- 函数体内没有隐式 this；访问非静态数据成员要通过 self。
- self 是名字约定，可按值、const 引用或转发引用接收对象。
- 不能声明为 static、virtual，函数尾部不能再写 cv/ref 限定符；显式对象形参
  不能带默认实参，也不能是函数形参包。
- 调用仍可写 obj.f()；它不因此变成能在命名空间随意定义的自由函数。
- 取地址时得到普通函数指针，例如 `&Widget::bump` 为 `void(*)(Widget&)`，
  调用该指针须显式传对象，与传统成员函数指针不同。

### 1.3 三类常见用途

1. 合并逻辑相同的 cv/ref 重载，并按推导出的类型选择行为。
2. 递归 lambda 用显式对象参数取得闭包本身，减少自传参包装。
3. 基类 mixin 通过泛型 self 推导派生类型，某些 CRTP 场景可省掉基类模板参数。
   这不取代虚函数动态多态，也不会绕过访问控制。

## 二、具体语法

```cpp
struct Widget {
    int value = 0;
    void bump(this Widget& self) { ++self.value; }
    int read(this const Widget& self) { return self.value; }

    decltype(auto) get(this auto&& self) {
        return (std::forward<decltype(self)>(self).value);
    }
};

auto fact = [](this auto self, int n) -> long long {
    return n <= 1 ? 1 : n * self(n - 1);
};

struct Query {
    int limit_ = 0;
    decltype(auto) limit(this auto&& self, int n) {
        self.limit_ = n;
        return std::forward<decltype(self)>(self);
    }
};
```

上述代码需要 `<utility>`；get 返回引用的调用还须遵守底层对象生命周期。
尤其 `auto&& r = Widget{}.get();` 不会延长通过函数返回的临时 Widget 的生命期。

规则细则：

- 命名的 self 通常作为左值表达式使用。要从成员搬出资源，需正确转发对象或
  使用 forward_like；仅推导成右值引用并不会使所有成员访问自动成为右值。
- 按值 self 从左值通常复制，从右值可能移动或省略构造；若固定为基类值类型，
  从派生对象调用还可能切片。不是“每次必定拷贝”。
- const 右值并不保证可移动资源：常见移动构造需要非 const 右值。
- 在成员 getter 中，decltype(auto) 的 `return self.value;` 是未加括号成员访问，
  会按成员声明类型推导；`return (self.value);` 通常得到左值引用。
  转发对象再访问成员并加括号，可保留所需 cv/ref。
- C++23 的隐式移动规则会影响 `return self;`，不能笼统断言其一定丢失值类别；
  链式代码显式 forward 更清楚，但也不延长对象寿命。

## 三、实际应用与最佳实践

### 3.1 场景：四种 cv/ref 组合

示例分别打印非 const 左值、const 左值、非 const 右值与 const 右值。
对重复的逻辑可合并，对语义确实不同的接口仍可保留独立重载或约束。

### 3.2 场景：链式构建器

limit/page 返回正确类别的对象引用，允许左值原地链与临时对象链。
`finish(this Query&&)` 仅限制必须从右值调用：std::move 也能把仍存活的 q
转换为右值表达式。它不会自动销毁 q、移动所有资源、防止再次调用，
也不能检查构建参数已齐全。需要这些保证时须另行设计状态或类型约束。

### 3.3 最佳实践清单

1. 先明确返回值/引用语义，再合并重载，并用 static_assert 验证四种返回类型。
2. 递归 lambda 的 self 按值可能复制捕获；含大对象或不可复制捕获时考虑引用。
3. 递归返回类型尚未推导时就调用自身会失败，显式返回类型通常更直接。
4. 虚函数接口保持传统成员形式；不要把显式对象形参当作动态分派机制。
5. 临时对象的链式结果引用不要逃出完整表达式。

## 常见陷阱

1. 写裸非静态成员或 this：函数没有隐式对象，需使用 self。
2. 固定 `this W&` 无法接收 const 或右值对象，不能当成万能版。
3. 把按值 self 等同于始终复制，忽略右值移动、省略构造和基类切片。
4. 沿用传统成员函数指针容器存 `&W::bump`，两种指针类型不兼容。
5. 把右值 finish 当作“只能调用一次”的静态保证，实际对象仍可能被复用。
6. getter 返回引用却引用了已经销毁的临时对象；forward 不提供所有权。

## 完整示例

```bash
g++ -std=c++23 -Wall -Wextra "CPP23/52_deducing_this/52_deducing_this.cpp" -o run -lstdc++exp && ./run
```

## 练习

1. 对 Widget getter 的四种对象类别分别断言 int&、const int&、int&&、const int&&。
2. 写递归 gcd，再比较按值 self 与引用 self 对带捕获闭包的复制影响。
3. 编写无需 CRTP 类型参数的 mixin，验证推导得到的派生类型与访问控制。
4. 对链式接口比较返回值、左值引用和转发引用的生命周期与所有权差异。

## 参考

- [P0847R7：Deducing this 的设计与语义](https://wg21.link/p0847r7)
- [C++23 草案：显式对象形参声明的限制](https://timsong-cpp.github.io/cppwp/n4950/dcl.fct)
