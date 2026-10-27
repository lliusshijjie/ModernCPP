// 第 07 课示例：decltype(auto) 与返回类型推导（C++14）
//
// 结构：① 原理验证 —— auto 剥引用 vs decltype(auto) 保引用（函数与变量）
//       ② 语法演示 —— 括号改变返回类型；cv/引用保留；move 表达式
//       ③ 应用实战 —— 完美转发包装；通用下标包装
//
// 编译运行：
//   g++ -std=c++14 -Wall -Wextra 07_decltype_auto.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <set>

// 演示数据
std::vector<int> v{10, 20, 30};

// ① 函数返回类型对比：v[i] 的类型是 int&
auto copy_val(std::vector<int>& c, std::size_t i) { return c[i]; }         // int 副本
decltype(auto) ref_val(std::vector<int>& c, std::size_t i) { return c[i]; } // int& 引用

static_assert(std::is_same<decltype(copy_val(v, 0)), int>::value, "auto -> int");
static_assert(std::is_same<decltype(ref_val(v, 0)), int&>::value, "decltype(auto) -> int&");

// ② 括号改变返回类型（全局变量演示，安全无悬空）
int g = 1;
decltype(auto) ret_entity() { return g; }    // g 是实体 → int（值）
decltype(auto) ret_paren()  { return (g); }  // (g) 是左值表达式 → int&（引用）

static_assert(std::is_same<decltype(ret_entity()), int>::value, "");
static_assert(std::is_same<decltype(ret_paren()), int&>::value, "");

// ③ 完美转发包装：保住被调函数的返回类别
int& pick(std::vector<int>& c, std::size_t i) { return c[i]; }

template <class F, class... Args>
decltype(auto) invoke_forward(F&& f, Args&&... args) {
    return std::forward<F>(f)(std::forward<Args>(args)...);
}

// ③ 通用下标包装：operator[] 保引用
template <class Container>
struct At {
    Container& c;
    template <class I>
    decltype(auto) operator[](I i) { return c[i]; }
};

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：auto 拿回的是副本，decltype(auto) 拿回真引用
    ref_val(v, 1) = 99;                     // 通过返回的引用直改容器
    std::cout << v[1] << '\n';              // 99
    std::cout << copy_val(v, 1) << '\n';    // 99（只是副本的值）

    // 验证 2：变量声明同理 —— auto 拷贝，decltype(auto) 成别名
    int x = 42;
    int& rx = x;
    auto        a = rx;      // int（拷贝）
    decltype(auto) b = rx;   // int&（别名）
    a = 7;
    b = 8;
    std::cout << x << ' ' << (a != x) << '\n';   // 8 true

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 括号改变一切：return g → int（值）；return (g) → int&
    ret_entity();          // 值返回：改不动 g
    ret_paren() = 2;       // 引用返回：改得动
    std::cout << g << '\n';     // 2

    // 2.2 cv 与引用的完整保留
    const int& cri = x;
    auto        a2 = cri;        // int（引用与 const 全剥）
    decltype(auto) b2 = cri;     // const int&
    static_assert(std::is_same<decltype(a2), int>::value, "");
    static_assert(std::is_same<decltype(b2), const int&>::value, "");
    std::cout << b2 << '\n';     // 8（读的是 x 当前值 —— 证明是别名）

    // 2.3 move 表达式 → T&&
    std::string s = "tmp";
    decltype(auto) t = std::move(s);     // std::string&&
    static_assert(std::is_same<decltype(t), std::string&&>::value, "");
    std::cout << (t == "tmp") << '\n';   // true（绑定引用，不移动任何东西）

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 完美转发包装：f 返回 int&，包装也返回 int&，可直接赋值
    invoke_forward(pick, v, 2) = 77;
    static_assert(std::is_same<decltype(invoke_forward(pick, v, 2)), int&>::value, "");
    std::cout << v[2] << '\n';               // 77

    // 3.2 通用下标包装：直写容器，零拷贝
    At<std::vector<int>> av{v};
    av[0] = 11;
    std::cout << av[0] << ' ' << v[0] << '\n';   // 11 11

    return 0;
}
