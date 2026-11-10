// 第 26 课示例：inline 变量与 ODR 的现代化（C++17）
//
// 结构：① 原理验证 —— static constexpr 成员的 odr-use；inline 变量单实体
//       ② 语法演示 —— inline constexpr 常量绑引用；可变 inline 全局
//       ③ 应用实战 —— header-only 库的全局计数器；Meyers 函数的对照
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 26_inline_variables.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <iostream>
#include <string>

#include "26_config.hpp"    // 迷你 header-only 库：inline 变量都在这里面

// ③ Meyers 函数对照：有初始化顺序依赖时的正解（局部 static 顺序确定）
int& legacy_counter() {
    static int c = 0;       // 首次调用时初始化 —— 顺序确定，懒加载
    return c;
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：类内 static constexpr 绑引用（odr-use）—— C++14 链接报错，
    //         C++17 隐式 inline 直接通过
    const double& r = Config::ratio;
    std::cout << r << '\n';                       // 0.5

    // 验证 2：inline 可变全局就放在头文件里 —— 引用别名与本体是同一实体
    //         （地址值不可预测不打印；跨 TU 地址相同的实验见练习 1）
    std::cout << api_calls << '\n';               // 0
    int& alias = api_calls;
    ++alias;                                      // 经别名修改
    std::cout << api_calls << '\n';               // 1 —— 别名即本体

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 inline constexpr 常量：唯一实体，绑引用安全
    const int& mr = max_retry;
    std::cout << mr << ' ' << (mr == 3) << '\n';  // 3 true

    // 2.2 动态初始化的 inline 常量（string 构造发生在初始化时，恰好一次）
    std::cout << app_version << ' ' << app_version.size() << '\n';  // v1.2.3 6

    // 2.3 对照：命名空间 constexpr 不隐式 inline —— 每 TU 各一份、地址可能不同
    //         （双 TU 实验见练习 4；要唯一地址必须显式 inline constexpr）

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 header-only 库的全局计数器：多处自增，全局可见
    auto call_a = [] { ++api_calls; };
    auto call_b = [] { ++api_calls; ++api_calls; };
    call_a();
    call_b();
    std::cout << api_calls << '\n';               // 3（1 + 2）

    // 3.2 与 Meyers 函数互操作：两条路改的是不同实体（教学对照），
    //     真实工程里选一种风格坚持
    ++legacy_counter();
    ++legacy_counter();
    std::cout << legacy_counter() << ' ' << api_calls << '\n';   // 2 3

    // 3.3 常量驱动的重试循环
    for (int i = 1; i <= max_retry; ++i) {
        std::cout << "try " << i << '\n';         // try 1 / try 2 / try 3
    }

    return 0;
}
