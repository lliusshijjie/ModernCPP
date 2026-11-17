// 第 52 课示例：deducing this：显式对象形参（C++23）
//
// 结构：① 原理验证 —— self 参数化；const/值类别自动推导
//       ② 语法演示 —— 一个函数替代四个重载；递归 lambda
//       ③ 应用实战 —— 链式构建器（右值收尾搬家）
//
// 本机（MSYS2 g++ 15.1.0）编译运行——注意链接 -lstdc++exp：
//   g++ -std=c++23 -Wall -Wextra 52_deducing_this.cpp -o run -lstdc++exp && ./run
// 先预测每行输出，再运行对照！

#include <iostream>
#include <print>
#include <type_traits>
#include <utility>

struct Widget {
    int value = 0;

    // ① 显式对象形参：没有隐式 this，self 是普通参数（名字随意）
    void bump(this Widget& self) { ++self.value; }

    // ① const 版：const Widget& self——调用方是 const 对象时命中
    int read(this const Widget& self) { return self.value; }

    // ② 一个函数替代四个重载：self 的 cv/值类别全部交给推导
    void classify(this auto&& self) {
        using S = std::remove_reference_t<decltype(self)>;
        if constexpr (std::is_lvalue_reference_v<decltype(self)>) {
            if constexpr (std::is_const_v<S>)
                std::println("lvalue-const");
            else
                std::println("lvalue-mut");
        } else {
            std::println("rvalue");
        }
    }
};

// ② 递归 lambda：deducing this 之前要写 std::function 或 Y 组合子
auto factorial = [](this auto self, int n) -> long long {
    return n <= 1 ? 1 : n * self(n - 1);      // self 就是"我自己"
};

// ③ 链式构建器：self 的值类别沿链穿透（左值链保持左值、右值链保持右值）
struct Query {
    int limit_ = 0, offset_ = 0;
    decltype(auto) limit(this auto&& self, int n) {
        self.limit_ = n;
        return std::forward<decltype(self)>(self);   // 原样转发值类别
    }
    decltype(auto) page(this auto&& self, int n) {
        self.offset_ = n * self.limit_;       // 复用已设字段
        return std::forward<decltype(self)>(self);
    }
    int finish(this Query&& self) {           // 收尾：要求右值（用完即弃）
        return self.offset_ + self.limit_;
    }
};

int main() {
    std::print("===== 1. PRINCIPLES =====\n");

    // 验证 1：显式 self 就是普通参数——bump 改的就是对象本体
    Widget w;
    w.bump();
    w.bump();
    std::println("{} {}", w.value, w.read());     // 2 2

    // 验证 2：const 对象走 const 版 self（read 可用，bump 编译错）
    const Widget cw{7};
    std::println("{}", cw.read());                // 7
    // cw.bump();   // 编译错：没有匹配的非 const self——constness 参与重载

    // 验证 3：显式对象成员里没有隐式 this（注释保存）
    // void wrong(this Widget& self) { ++value; }   // 编译错：value 需要 self.

    std::print("===== 2. SYNTAX =====\n");

    // 2.1 四种调用形态 -> 一个函数（旧世界要写四个重载）
    Widget a, b;
    const Widget& cb = b;
    a.classify();            // lvalue-mut
    cb.classify();           // lvalue-const
    Widget{}.classify();     // rvalue
    std::move(a).classify(); // rvalue

    // 2.2 递归 lambda
    std::println("{}", factorial(10));             // 3628800

    // 2.3 值语义成员（按值 self）——适合小型只读视图/代理
    struct Snap {
        int v = 0;
        int twice(this Snap self) { return self.v * 2; }   // 拷贝一份算
    };
    std::println("{}", Snap{21}.twice());          // 42

    std::print("===== 3. APPLICATION =====\n");

    // 3.1 链式构建器：设置字段 -> 收尾强制右值（防半成品留存）
    Query q;
    q.limit(10).page(3);
    std::println("{}", std::move(q).finish());     // 40（offset 30 + limit 10）
    // Query{}.limit(5).page(2).finish() 也可以：
    std::println("{}", Query{}.limit(5).page(2).finish());   // 15

    // finish 要求右值：lvalue 上直接调用编译错（注释保存）
    // Query q2; q2.finish();   // 编译错：finish 需要 Query&&

    // 3.2 实战心法（对照 ①②③）：
    //   const/&/&& 的组合爆炸 -> 一个 this auto&& + if constexpr
    //   递归 lambda 一行到位（std::function 退役）
    //   "收尾搬家"语义用显式右值 self 表达（半成品不可留存）
    return 0;
}
