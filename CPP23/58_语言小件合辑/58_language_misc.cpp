// 第 58 课示例：语言小件合辑：if consteval、static operator()、多维下标（C++23）
//
// 结构：① 原理验证 —— if consteval 双路径；立即对象 static operator()
//       ② 语法演示 —— 多维 operator[]；与逗号陷阱的对照
//       ③ 应用实战 —— 编译期/运行期双优的算法 + 立即可调用对象
//
// 本机（MSYS2 g++ 15.1.0）编译运行——注意链接 -lstdc++exp：
//   g++ -std=c++23 -Wall -Wextra 58_language_misc.cpp -o run -lstdc++exp && ./run
// 先预测每行输出，再运行对照！

#include <cmath>
#include <print>
#include <type_traits>

// ① if consteval：编译期/运行期双路径（is_constant_evaluated 的语法化）
constexpr double safe_sqrt(double x) {
    if consteval {                       // 常量求值语境（编译期）
        return x < 1.0 ? 0.0 : 1.0;      // 编译期保守路径（示意：不依赖 libm）
    } else {                             // 运行期
        return std::sqrt(x);             // libm 快路径
    }
}

// ① static operator()：无状态的立即可调用对象
struct Square {
    static int op(int x) { return x * x; }
    static constexpr double operator()(double x) { return x * x; }  // C++23
};

// ② 多维 operator[]：矩阵不必再装 pair/逗号
struct Mat2x2 {
    double m[4] = {1, 2, 3, 4};
    double& operator[](std::size_t r, std::size_t c) { return m[r * 2 + c]; }
    const double& operator[](std::size_t r, std::size_t c) const {
        return m[r * 2 + c];
    }
};

int main() {
    std::print("===== 1. PRINCIPLES =====\n");

    // 验证 1：双路径——编译期与运行期各走各的
    static_assert(safe_sqrt(0.25) == 0.0);       // 编译期：保守路径给 0
    std::print("{} {}\n", safe_sqrt(0.25),       // 0.5（运行期：libm）
                safe_sqrt(4.0));                  // 2

    // 对照旧写法：if (std::is_constant_evaluated())——C++20 的库函数版
    // （if consteval 是它的语法化升级：误用场景更少、分支可直接用编译期设施）

    // 验证 2：static operator()——函数对象无 this
    Square s{};
    std::print("{} {} {}\n", s(1.5), Square{}(2.5),  // 2.25 6.25（两种调用）
                s.op(3));                            // 9（普通静态成员）

    std::print("===== 2. SYNTAX =====\n");

    // 2.1 多维下标：语义直接
    Mat2x2 a;
    a[1, 0] = 30;                                   // 第 1 行第 0 列
    std::print("{} {} {}\n", a[0, 0], a[1, 0], a[1, 1]);   // 1 30 4

    // 2.2 老代码的逗号陷阱仍在：原生数组/未重载类型的 m[1,0] 是 m[0]
    double raw[4] = {1, 2, 3, 4};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcomma-subscript"
    std::print("{}\n", raw[1, 2]);                  // 3（= raw[2]！不是"行1列2"）
#pragma GCC diagnostic pop
    // 逗号表达式取后者：raw[(1, 2)] = raw[2]。多维 [] 只对重载了
    // 多参 operator[] 的类型（如 Mat2x2）生效；C++23 起编译器对
    // "顶层逗号下标"发警告（-Wcomma-subscript）敦促清理

    // 2.3 static operator() 与 constexpr/模板的组合
    static_assert(Square{}(3.0) == 9.0);            // 编译期调用立即可调用对象
    std::print("static assert ok\n");

    std::print("===== 3. APPLICATION =====\n");

    // 3.1 立即可调用对象做定制点（模板形参当策略，不用函数指针）
    struct ByTwice {
        static int operator()(int x) { return x * 2; }
    };
    struct BySquare {
        static int operator()(int x) { return x * x; }
    };
    auto apply = [](auto op, int v) { return op(v); };      // 值传"策略对象"
    std::print("{} {}\n", apply(ByTwice{}, 21),     // 42
                apply(BySquare{}, 5));              // 25

    // 3.2 双路径实战心法（对照 ①②③）：
    //   is_constant_evaluated 时代结束——新代码一律 if consteval
    //   零状态函数对象加 static：更明确、可 constexpr、无 this 开销
    //   多维下标配上 -Wcomma-subscript 清理，老坑不再新踩
    return 0;
}
