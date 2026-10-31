// 第 13 课示例：if constexpr 编译期分支（C++17）
//
// 结构：① 原理验证 —— 丢弃分支不实例化：各类型只见自己的分支
//       ② 语法演示 —— 递归终点并入函数体；与 init 语句叠加；else if constexpr 链
//       ③ 应用实战 —— std::advance 一体化重写；按类型渲染
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 13_if_constexpr.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <iostream>
#include <iterator>
#include <list>
#include <string>
#include <type_traits>
#include <vector>

// ① 核心演示：else 分支要求 x 有 describe() 成员
//    对 int 而言这个分支是"非法的"，但它是丢弃分支 —— 不实例化，不检查
struct Widget {
    std::string describe() const { return "widget"; }
};

template <class T>
auto describe(const T& x) {
    if constexpr (std::is_integral_v<T>) {
        return std::string("integral:");      // 整型走这里
    } else {
        return x.describe();                  // 非整型才实例化这条（换成普通 if 立即编译错）
    }
}

// ② 递归终点并入函数体：不再需要 fact<0> 的独立重载
template <int N>
constexpr unsigned long long fact() {
    if constexpr (N <= 1) {
        return 1;
    } else {
        return N * fact<N - 1>();
    }
}

// ② else if constexpr 链：编译期类型 switch
template <class T>
std::string kind(T&&) {
    if constexpr (std::is_integral_v<T>) {
        return "int";
    } else if constexpr (std::is_floating_point_v<T>) {
        return "float";
    } else {
        return "other";
    }
}

// ③ std::advance 的 C++17 一体化实现
//    （C++98 需要 3 个 tag dispatch 重载；C++11 需要 SFINAE）
template <class It, class Dist>
void my_advance(It& it, Dist n) {
    if constexpr (std::is_same_v<typename std::iterator_traits<It>::iterator_category,
                                 std::random_access_iterator_tag>) {
        it += n;                              // 随机访问：一步到位
    } else {
        for (Dist i = 0; i < n; ++i) ++it;    // 双向/前向：逐个前进
    }
}

// ③ 按类型渲染：数字加括号，字符串原样
template <class T>
std::string render(const T& x) {
    if constexpr (std::is_arithmetic_v<T>) {
        return "[" + std::to_string(x) + "]";
    } else {
        return x;
    }
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：同一模板，两种类型各走各的分支
    std::cout << describe(42) << '\n';        // integral:
    std::cout << describe(Widget{}) << '\n';  // widget

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 编译期阶乘：终点在函数体内
    static_assert(fact<5>() == 120, "");
    std::cout << fact<10>() << '\n';          // 3628800

    // 2.2 与 init 语句叠加：init 变量要参与常量条件，自身必须 constexpr
    //    （写成普通 int n 的话，"n > 4" 不是常量表达式 —— 编译错误）
    if constexpr (constexpr int n = 2 + 3; n > 4) {
        std::cout << "compile-time known " << n << '\n';   // compile-time known 5
    }

    // 2.3 else if constexpr 链
    std::cout << kind(1) << kind(1.5) << kind("x") << '\n';  // intfloatother

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 同一个 my_advance，随机访问迭代器走 +=，双向迭代器走循环
    std::vector<int> v{1, 2, 3, 4, 5};
    auto vit = v.begin();
    my_advance(vit, 3);
    std::cout << *vit << '\n';                // 4

    std::list<int> lst{1, 2, 3, 4, 5};
    auto lit = lst.begin();
    my_advance(lit, 2);
    std::cout << *lit << '\n';                // 3

    // 3.2 按类型渲染
    std::cout << render(42) << ' ' << render(std::string("cpp")) << '\n';  // [42] cpp

    return 0;
}
