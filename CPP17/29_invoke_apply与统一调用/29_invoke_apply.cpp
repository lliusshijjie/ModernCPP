// 第 29 课示例：std::invoke / std::apply 与统一调用（C++17）
//
// 结构：① 原理验证 —— INVOKE 协议统一四类可调用物（含成员数据指针）
//       ② 语法演示 —— apply 解包 tuple/pair/array；apply 走 INVOKE 语义
//       ③ 应用实战 —— 完美转发统一调用层；延迟任务队列
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 29_invoke_apply.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <array>
#include <functional>
#include <iostream>
#include <tuple>
#include <utility>
#include <vector>

struct Widget {
    int v;
    int add(int x) const { return v + x; }
};

int add2(int a, int b) { return a + b; }
int add3(int a, int b, int c) { return a + b + c; }

// ③ 完美转发的统一调用层：第 07 课的转发 + 本课的 invoke
//    —— 任何 callable（含成员指针）+ 任何实参，一次写对
template <class F, class... Args>
decltype(auto) call(F&& f, Args&&... args) {
    return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    Widget w{40};
    auto lam = [](int a, int b) { return a + b; };

    // 验证 1：普通函数 / lambda —— invoke 与裸调用等价
    std::cout << std::invoke(add2, 1, 2) << ' '
              << std::invoke(lam, 3, 4) << '\n';            // 3 7

    // 验证 2：成员函数指针 —— 对象是"第一实参"（对象/指针皆可）
    auto pmf = &Widget::add;
    std::cout << std::invoke(pmf, w, 2) << ' '
              << std::invoke(pmf, &w, 2) << '\n';           // 42 42

    // 验证 3：成员数据指针 —— invoke 返回成员的引用，可读可写
    auto pmd = &Widget::v;
    std::invoke(pmd, w) = 50;                // 通过"调用"写成员
    std::cout << w.v << ' ' << std::invoke(pmd, w) << '\n'; // 50 50

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 apply：tuple 拆开当实参（第 03 课手写 impl 的标准答案）
    std::cout << std::apply(add3, std::make_tuple(1, 2, 3)) << '\n';   // 6

    // 2.2 apply 走 INVOKE 语义：成员指针 + 对象打包的 tuple 也行
    std::cout << std::apply(&Widget::add, std::make_tuple(w, 2))
              << '\n';                                      // 52（w.v 已是 50）

    // 2.3 pair / array 同样是 tuple-like
    std::cout << std::apply(add2, std::make_pair(7, 8)) << ' '
              << std::apply(add3, std::array<int, 3>{4, 5, 6})
              << '\n';                                      // 15 15

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 统一调用层：函数、lambda、成员指针一视同仁
    std::cout << call(add2, 5, 6) << ' '
              << call(lam, 7, 8) << ' '
              << call(&Widget::add, w, 8) << '\n';          // 11 15 58

    // 3.2 延迟任务队列：注册时 tuple 打包，执行时 apply 拆包
    //    —— 任意 callable/参数组合统一为可存储的闭包
    std::vector<std::function<int()>> tasks;
    auto submit = [&tasks](auto f, auto packed) {
        tasks.push_back([f, packed] { return std::apply(f, packed); });
    };
    submit(add2, std::make_pair(10, 20));                   // 存：稍后算 10+20
    submit(&Widget::add, std::make_tuple(Widget{1}, 2));    // 存：成员调用
    for (const auto& t : tasks) std::cout << t() << ' ';
    std::cout << '\n';                                      // 30 3

    return 0;
}
