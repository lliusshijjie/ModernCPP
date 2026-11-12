// 第 28 课示例：求值顺序固定 P0145（C++17）
//
// 结构：① 原理验证 —— << 链的定序；赋值 RHS 先于 LHS
//       ② 语法演示 —— 后缀表达式先于实参
//       ③ 应用实战 —— 第04课泄漏缺口的关闭（构造数 == 析构数）
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 28_evaluation_order.cpp -o run && ./run
// 先预测每行输出，再运行对照！
//（对照实验：-std=c++14 重编译，① 部分即为 UB —— 输出可能不同）

#include <iostream>
#include <memory>

// ② 成员调用的定序演示：make_rec() 是后缀表达式，先于实参求值
struct Rec {
    int take(int x) const { return x; }
};
Rec make_rec(int id) {
    std::cout << "rec" << id << ' ';
    return Rec{};
}
int produce(int v) {
    std::cout << "arg" << v << ' ';
    return v;
}

// ③ 泄漏缺口验证用的 Widget：数构造/析构
struct Widget {
    static int ctor, dtor;
    Widget() { ++ctor; }
    ~Widget() { ++dtor; }
};
int Widget::ctor = 0;
int Widget::dtor = 0;

void accept(std::unique_ptr<Widget>, int) {}

int may_throw(bool yes) {
    if (yes) throw 1;
    return 0;
}

int main() {
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 注意：下面三行在 C++17 下良定义，但 GCC 的 -Wsequence-point 告警器
    // 保守起见仍报警（它不追踪 C++17 定序规则）——"语言合法但工具链讨厌"
    // 正是这些写法的真实写照，pragma 局部压制以保证本文件零警告。
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsequence-point"

    // 验证 1：<< 链 E1 先于 E2（C++17 定序；C++14 是 UB）
    int i = 0;
    std::cout << i << i++ << '\n';     // 00：先求 (cout<<i) 打印 0，再求 i++（值 0）

    // 验证 2：调换位置 —— i++ 先完成（打出旧值 0，i 变 1），再读 i（1）
    int k = 0;
    std::cout << k++ << k << '\n';     // 01

    // 验证 3：赋值右操作数先于左操作数 —— j++ 完成后（j=1，值为 0）才赋给 j
    int j = 0;
    j = j++;
    std::cout << j << '\n';            // 0（C++17 良定义；C++14 是 UB）
#pragma GCC diagnostic pop

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 后缀表达式先于实参：make_rec() 全部完成，才轮到 produce()
    int r = make_rec(1).take(produce(7));
    std::cout << "| r=" << r << '\n';  // rec1 arg7 | r=7

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 第04课的泄漏缺口已关闭：实参不交错 ——
    //     若 uptr 实参先求值：new+构造+接管完整完成，may_throw 抛出时
    //     栈展开正常析构（dtor 跟上 ctor）；
    //     若 may_throw 先求值：对象根本没创建。
    //     两种顺序都不可能泄漏 —— "交错"这个第三态被 P0145 消灭。
    //     实参谁先谁后仍未指定，所以只断言 ctor == dtor（不打印具体次数）。
    for (int n = 0; n < 50; ++n) {
        try {
            accept(std::unique_ptr<Widget>(new Widget), may_throw(n % 2 == 0));
        } catch (int) {
            // 第偶数次抛异常：栈展开 —— 已构造的 Widget 在此被析构
        }
    }
    std::cout << std::boolalpha
              << "ctor==dtor: " << (Widget::ctor == Widget::dtor)
              << " (ctor=" << Widget::ctor << ")\n";
    // ctor==dtor: true (ctor=25) —— GCC 先求值右实参 may_throw：
    // 抛异常的 25 次里 Widget 压根没构造；非抛的 25 次构造+析构成对。
    // 换个先求左实参的编译器则 ctor=50 —— 顺序未指定，但零泄漏恒成立

    return 0;
}
