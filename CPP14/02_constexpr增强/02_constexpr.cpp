// 第 02 课示例：constexpr 增强（C++14）
//
// 结构：① 原理验证 —— constexpr 函数 = "可以在编译期运行的普通函数"
//       ② 语法演示 —— 局部变量 / 循环 / 修改局部对象 / 多 return
//       ③ 应用实战 —— 编译期生成查找表；同一函数兼跑运行期
//
// 编译运行：
//   g++ -std=c++14 -Wall -Wextra 02_constexpr.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <iostream>

// ============================================================
// ① 原理验证
// 这个函数在 C++11 下根本无法编译（局部变量/循环/修改全被禁止），
// 当时只能用模板递归；C++14 起 constexpr 就是"可编译期求值的普通函数"
constexpr int factorial(int n) {
    int result = 1;                  // C++11: 编译错误 —— 不允许局部变量
    for (int i = 2; i <= n; ++i) {   // C++11: 编译错误 —— 不允许循环
        result *= i;                 // C++11: 编译错误 —— 不允许修改
    }
    return result;
}

// 验证：C++14 起 constexpr 成员函数不再隐式 const，
// 编译期可以修改"本次求值中创建"的对象状态
struct Gauge {
    int v = 0;
    constexpr void tick(int by) { v += by; }   // C++11: 非 const 的 constexpr 成员非法
    constexpr int value() const { return v; }
};

constexpr int measure() {
    Gauge g;          // 局部对象：C++11 不允许
    g.tick(30);       // 修改对象状态：C++11 不允许
    g.tick(12);
    return g.value();
}

// 编译期求值的触发条件：实参为常量表达式，且结果被用于
// 需要常量表达式的上下文（constexpr 变量 / 数组维度 / 模板实参 / static_assert）
static_assert(factorial(5) == 120, "compile-time evaluation");   // 编译期执行
static_assert(factorial(10) == 3628800, "");
static_assert(measure() == 42, "compile-time mutation");         // 编译期执行

// while 循环遍历字符串（供 ② 使用）
constexpr int str_len(const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;        // C++14: while OK
    return n;
}

constexpr int sign(int x) {
    if (x > 0) return 1;             // 多条 return：C++14 OK
    if (x < 0) return -1;
    return 0;
}

// ============================================================
// ③ 应用实战
// 编译期生成阶乘查找表 —— 以前需要模板递归或外部脚本，现在一个循环搞定。
//
// 真实的 C++14 边界：std::array 的"非 const operator[]"到 C++17 才标记
// constexpr（const 重载才是 C++14），所以 C++14 无法在编译期写入
// std::array 元素。解法：用内建数组的字面类型包装（顺带演示 constexpr
// 成员函数与字面类型）；C++17 起可以直接用 std::array（见 CPP17/）。
struct FactorialTable {
    int v[11] = {};                                        // 0! .. 10!
    constexpr int& at(int i) { return v[i]; }              // 可写访问
    constexpr int operator[](int i) const { return v[i]; } // 只读访问
};

constexpr FactorialTable make_factorial_table() {
    FactorialTable t;                     // 字面类型：编译期可创建
    t.at(0) = 1;
    for (int i = 1; i < 11; ++i) {
        t.at(i) = t.at(i - 1) * i;
    }
    return t;
}

// 故意非 constexpr 的运行期接口：随机输入也走查表，表本身仍是编译期产物
int lookup_factorial(int n) {
    constexpr FactorialTable table = make_factorial_table();
    return table[n];
}

int main() {
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 普通调用：实参是常量，但结果没用于常量上下文
    // → 标准不要求编译期执行；constexpr 是"可能编译期"，不是"必须编译期"
    std::cout << factorial(5) << '\n';                          // 120

    // 验证：实参换成 volatile（运行期才知道的值）→ 一定是运行期调用，照样正确
    volatile int runtime_n = 5;
    std::cout << factorial(runtime_n) << '\n';                  // 120

    // 验证：结果用于数组维度（需要常量表达式）→ 强制编译期求值
    int table[factorial(6) / factorial(5)];   // 720/120 = 6 个元素
    table[0] = factorial(5);
    std::cout << sizeof(table) / sizeof(table[0]) << ' ' << table[0] << '\n';  // 6 120

    // 编译期修改对象状态：measure() 已被上面的 static_assert 证实是编译期算的
    std::cout << measure() << '\n';                             // 42

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 循环遍历（顶部 factorial 演示了 for，这里演示 while）
    std::cout << str_len("constexpr") << '\n';                  // 9

    // 2.2 多 return / if 分支
    std::cout << sign(-7) << ' ' << sign(0) << ' ' << sign(9) << '\n';   // -1 0 1

    // 2.3 const vs constexpr：
    //     const     = "初始化后不可修改"，可以用运行期值初始化
    //     constexpr = "必须编译期算出"，只能用常量表达式初始化
    const int ca = runtime_n + 1;      // 合法：运行期初始化的 const
    constexpr int cb = factorial(4);   // 必须是常量表达式
    std::cout << cb << ' ' << ca << '\n';                       // 24 6

    // 2.4 C++14 仍不允许（写进函数体会编译失败，只列不写）：
    //     static/thread_local 局部变量、goto、try-catch、
    //     调用非 constexpr 函数、未初始化的局部变量

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 编译期查找表：结果接 constexpr 变量 → 强制编译期生成
    constexpr auto ft = make_factorial_table();
    static_assert(ft[5] == 120 && ft[10] == 3628800, "table built at compile time");
    for (int i = 0; i <= 10; ++i) std::cout << ft[i] << ' ';
    std::cout << '\n';      // 1 1 2 6 24 120 720 5040 40320 362880 3628800

    // 3.2 同一份代码两个世界：ask 是运行期值 → 走运行期查表路径
    volatile int ask = 7;
    std::cout << lookup_factorial(ask) << '\n';                 // 5040

    // 3.3 最佳实践清单：
    //  a) 想强制编译期求值 → constexpr 变量 / static_assert / 模板实参，三选一
    //  b) 纯函数尽量标 constexpr：写一次，编译期/运行期两用
    //  c) 用 static_assert 给 constexpr 函数写"单元测试"，错误在编译期暴露
    //  d) 编译期求值打不了断点 —— 保留运行期调用路径便于调试
    return 0;
}
