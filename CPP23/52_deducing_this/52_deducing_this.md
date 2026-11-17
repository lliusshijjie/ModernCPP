# 第 52 课 · deducing this：显式对象形参（C++23）

> 所需标准：**C++23**（`-std=c++23`；本机同前课需 `-lstdc++exp` 链接）｜
> 讲解：本文件 ｜ 独立代码：`52_deducing_this.cpp`
> 前置：第 41 课（模板 lambda——self 推导同款机制）；第 27 课（值类别）

## 一、底层原理

### 1.1 this 的最后一个不对称

lambda 早已能泛型（第 01/41 课）、函数模板早已能推导参数——
唯独**成员函数的自己（this）**被钉死成 `T* const`。想为
const/非 const、左值/右值提供不同行为，只能写**四个重载**：
`f()` / `f() const` / `f() &` / `f() &&`——逻辑重复、维护爆炸。

C++23（P0847）允许把"自己"写成**第一个显式模板形参**：

```cpp
void f(this Widget& self);      // 旧：void f() —— this 隐式
void g(this auto&& self);       // 自己也是可推导的参数
```

调用 `obj.f()` 时，`obj` 被推导绑定给 `self`——
**cv 限定与值类别全部由推导获得**，一个函数覆盖四种形态。

### 1.2 显式 self 意味着什么

- `this` 关键字在该函数体内**不存在**：访问成员必须 `self.x`
- `self` 是普通绑定——可以按值、按 const 引用、按转发引用，
  甚至可以不叫 self（名字随意，self 是习惯）
- **不能是 virtual**（虚表机制建立在隐式 this 上）；
  函数本身也不能再有 cv/ref 限定（限定都写在 self 上）
- 静态成员函数的"对象版"缺口被填上：以前想要"既能这样又能那样"
  只能拆成自由函数

### 1.3 三大招牌应用

1. **重载塌缩**：四个 cv/ref 组合 → 一个 `this auto&&` +
   `if constexpr`（示例 ②）
2. **递归 lambda**：`[](this auto self, int n)`——self 把
   lambda 自己传进来，`std::function`/Y 组合子的样板全部退休（示例 ②）
3. **CRTP 的替代**：旧世界"静态多态"要写
   `template <class D> struct base { D& self() {...} }`；
   显式对象形参让基类直接拿到**推导后的派生类型**，
   mixin/链式接口不再需要 CRTP 模板层（MD 速览，示例不展开）

## 二、具体语法

```cpp
struct W {
    void bump(this W& self) { ++self.value; }        // 左值：修改
    int  read(this const W& self);                   // const 对象可用
    void probe(this auto&& self) {                   // 万能版：值类别+cv 全推导
        if constexpr (std::is_lvalue_reference_v<decltype(self)>)
            /* 左值分支 */;
        else
            /* 右值分支（可 move 出资源）*/;
    }
};

auto fact = [](this auto self, int n) -> long long { // 递归 lambda
    return n <= 1 ? 1 : n * self(n - 1);
};

// 链式接口：值类别沿链穿透
decltype(auto) limit(this auto&& self, int n) {
    self.limit_ = n;
    return std::forward<decltype(self)>(self);       // 左值链/右值链各自延续
}
```

规则细则：

- 显式对象形参必须是**第一个**参数，且每个成员函数至多一个
- 该函数内**没有隐式 this**：`value` 要写 `self.value`
- 不能 virtual、不能同时带 cv/ref 限定符、不能 static
- 按值 self（`this W self`）会**拷贝对象**——适合小型只读代理，
  修改不影响原对象（示例 ② 的 Snap）
- 显式对象成员**不取地址进重载集的传统位置**：
  `&W::bump` 的类型是普通函数指针（`void(*)(W&)`）而非成员指针——
  与旧成员指针互不通用
- lambda 的显式对象形参推导自身类型时**不引入无限递归**：
  推导发生在闭包类型确定之后

## 三、实际应用与最佳实践

### 3.1 场景：const 正确性与值类别语义（示例 ①/②）

"getter 要 const 版、builder 要 && 收尾"——旧世界四重载；
新世界一个 `this auto&&`：const 对象自动只读、右值自动可搬。
示例 ② 的 `classify` 把四种调用形态打到一行输出，直观验证推导。

### 3.2 场景：链式 DSL/构建器（示例 ③）

`q.limit(10).page(3)` 与 `Query{}.limit(5).page(2).finish()`——
左值链保持原地、右值链保持临时，`finish(this Query&&)` 强制
"收尾必须在右值上"，半成品对象拿不到结果（编译期拦截误用）。

### 3.3 最佳实践清单

1. 新代码里 const/非 const 重载对 → 改 `this auto&&` 一个版本
2. 递归 lambda 用 `this auto self`；按需传引用版（大对象）
3. 链式接口沿链 `forward` 值类别；终结操作要求 `this T&&`
4. 需要虚函数多态的成员**不要**改成显式对象形参（不兼容）
5. 值语义小工具（视图、代理）用按值 self 简化心智

## 常见陷阱

1. **函数体内写裸成员**（忘了没有隐式 this）：
   `++value;` 编译错——`++self.value;` 才是显式世界的写法
2. `this W self` 按值**拷贝**了对象：修改丢了、还多付一次拷贝——
   想改原对象用 `this W&`
3. 显式对象成员**不能 virtual**：把它放进虚接口 = 编译错；
   多态体系里的成员保持传统写法
4. 旧成员指针与显式对象成员的指针**类型不同**：
   `void(*)(W&)` vs `void(W::*)()`——按旧习惯存成员函数指针的
   框架代码要注意
5. 链式返回 `self`（裸 return）在 `this auto&&` 版里丢值类别：
   必须 `std::forward<decltype(self)>(self)`，
   否则右值链半路退化为左值/悬垂引用
6. lambda 递归忘写返回类型：`[](this auto self, int n)` 体内
   `self(n-1)` 需要已知返回类型，省略 `-> long long` 会推导失败

## 完整示例

见 `52_deducing_this.cpp`，按 ① 原理验证 → ② 语法演示 → ③ 应用实战 三段组织：

```bash
g++ -std=c++23 -Wall -Wextra "CPP23/52_deducing_this/52_deducing_this.cpp" -o run -lstdc++exp && ./run
```

## 练习

1. 把示例 `classify` 扩展出第五种形态：`const Widget&&`
   （提示：is_const + 非左值组合），验证输出。
2. 用递归 lambda 写欧几里得 gcd；再写带记忆化的 fib——
   self 参数外再套一个闭包捕获 map（对照第 01 课初始化捕获）。
3. 把你项目里某对 const/非 const 重载改成 `this auto&&` 单版本，
   用 static_assert 验证两种调用返回类型的 const 性正确。
4. 用显式对象形参重写一个小 CRTP mixin（如 `add_print`），
   对比行数与可读性。

## 参考

- 提案 P0847R7（Deducing this）
- cppreference: [显式对象形参（deducing this）](https://zh.cppreference.com/w/cpp/language/member_functions#显式对象形参)
- Gašper Ažman & Sy Brand 的教学文章（提案作者写的入门材料）
