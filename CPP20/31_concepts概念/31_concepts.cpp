// 第 31 课示例：Concepts 概念（C++20）—— CPP20 篇开篇
//
// 结构：① 原理验证 —— concept 是命名的编译期谓词；标准库 concepts
//       ② 语法演示 —— 三种使用姿势；requires 四件套
//       ③ 应用实战 —— 约束化 API；subsumption 自动优选重载
//
// 编译运行：
//   g++ -std=c++20 -Wall -Wextra 31_concepts.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <concepts>
#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

// ① 简单要求：表达式合法即可（不求值！）
template <class T>
concept Addable = requires(T a, T b) { a + b; };

// ①/③ 组合标准库 concepts
template <class T>
concept Numeric = std::integral<T> || std::floating_point<T>;

// ② 四件套：类型要求 / 复合要求 / 嵌套要求（+ 简单要求见 Addable）
template <class T>
concept Container = requires(T c, const T cc) {
    typename T::value_type;                                  // ① 类型要求
    { c.size() } -> std::convertible_to<std::size_t>;        // ② 复合要求
    { cc.empty() } -> std::same_as<bool>;                    // ② 复合要求
    requires std::is_default_constructible_v<T>;             // ③ 嵌套要求
};

// ② 三种使用姿势（约束力相同，位置不同）
template <Addable T>                     // 姿势一：直接当"类型"
T add1(T a, T b) { return a + b; }

template <class T> requires Numeric<T>   // 姿势二：requires 子句
T twice(T x) { return x + x; }

void print_num(Numeric auto x) {         // 姿势三：auto 简写
    std::cout << x << '\n';
}

// ③ subsumption：同名重载按约束包含自动优选——tag 派发的终结者
template <class T>
T pick(T a, T /*b*/) { return a; }                 // 兜底版（无约束）

template <std::integral T>
T pick(T /*a*/, T b) { return b; }                 // integral 更严：对整数胜出

// ③ 约束化的 API：错误信息一行看懂
template <class... Ts> requires (Numeric<Ts> && ...)
auto sum(Ts... xs) { return (xs + ... + 0); }      // 折叠（第 14 课）

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：concept 是编译期谓词 —— int 可加、两个 string 指针不可加
    static_assert(Addable<int>);
    static_assert(!Addable<std::string*>);        // ptr + ptr 非法
    std::cout << Addable<int> << ' ' << Addable<std::string*>
              << '\n';                            // true false

    // 验证 2：标准库 concepts 直接可用
    static_assert(std::integral<int> && !std::integral<double>);
    std::cout << Numeric<int> << ' ' << Numeric<std::string>
              << '\n';                            // true false

    // 验证 3：requires 表达式不求值（只验合法性）——
    //         Addable 的 a + b 从未真正执行过任何运算

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 姿势一：类型位置
    std::cout << add1(2, 3) << ' '
              << add1(std::string("ab"), std::string("cd"))
              << '\n';                            // 5 abcd

    // 2.2 姿势三：auto 简写（42 与 3.5 都满足 Numeric）
    print_num(42);                                // 42
    print_num(3.5);                               // 3.5
    // print_num("x");   // 编译错（一行讲清原因）：
    //   constrained by 'Numeric auto' ... constraints not satisfied

    // 2.3 四件套 concept 的判定
    static_assert(Container<std::vector<int>>);
    static_assert(!Container<int>);               // 无 value_type/size/empty
    std::cout << Container<std::vector<int>> << ' ' << Container<int>
              << '\n';                            // true false

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 约束化 API：sum 拒绝非数值（编译期），数值畅行
    std::cout << sum(1, 2, 3) << ' ' << sum(1.5, 2.5) << '\n';   // 6 4
    // sum(std::string("a"), "b");  // 编译错：constraints not satisfied
    // 对比 enable_if 时代的报错：数千行模板展开（此处省略）

    // 3.2 subsumption：int 落到 integral 版（返回 b），double 落兜底版（返回 a）
    std::cout << pick(1, 2) << ' ' << pick(1.5, 2.5) << '\n';    // 2 1.5

    // 3.3 姿势二的调用：twice 要求 Numeric
    std::cout << twice(21) << ' ' << twice(1.5) << '\n';         // 42 3

    return 0;
}
