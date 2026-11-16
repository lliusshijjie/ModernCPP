// 第 40 课示例：consteval、constinit 与 std::source_location（C++20）
//
// 结构：① 原理验证 —— 立即函数的三条铁律；constinit 的"常量初始化"
//       ② 语法演示 —— source_location 默认实参捕获调用方位置
//       ③ 应用实战 —— 无宏日志；编译期查表
//
// 编译运行：
//   g++ -std=c++20 -Wall -Wextra 40_consteval_constinit_source.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <array>
#include <iostream>
#include <source_location>
#include <string_view>

// ① consteval：立即函数——每次调用都必须是编译期常量
consteval int sq(int x) { return x * x; }

// ① constinit：静态存储 + 常量初始化（可运行期修改！）
constinit int counter = 7;

// ③ 编译期查表：consteval 造表，constexpr 接住（具体存储段由实现决定）
consteval std::array<int, 4> squares() {
    std::array<int, 4> r{};
    for (int i = 0; i < 4; ++i) r[i] = i * i;
    return r;
}
constexpr auto kSquares = squares();

// ③ 无宏日志：默认实参在"调用方"处求值
void log(std::string_view msg,
         std::source_location loc = std::source_location::current()) {
    std::cout << "[L" << loc.line() << ' ' << loc.function_name()
              << "] " << msg << '\n';
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：consteval 结果进编译期——static_assert 直接用
    static_assert(sq(3) == 9);
    std::cout << kSquares[3] << '\n';            // 9

    // 验证 2：立即调用可嵌在运行期表达式里（那一段仍是编译期算的）
    int base = 5;                                // 运行期值
    std::cout << sq(2) * 10 + base << '\n';      // 45（sq(2)=4 编译期定死）
    // int bad = sq(base);   // 编译错：base 不是常量 —— consteval 的铁律

    // 验证 3：constinit 可修改——"编译期初始化"不等于"只读"
    ++counter;                                   // 运行期改，合法
    std::cout << counter << '\n';                // 8

    // constinit 的反面（编译错示例，注释保存）：
    // int runtime_init();
    // constinit int bad = runtime_init();   // 编译错：初始化器须为常量表达式
    // constinit int local = 1;              //（放在函数内）编译错：仅限
    //                                        // 静态/线程存储期变量
    static constinit int local_counter = 1;       // 函数内 static 合法
    ++local_counter;

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 source_location 四件套：line / column / file_name / function_name
    auto loc = std::source_location::current();
    std::cout << loc.function_name() << ' ' << (loc.line() > 0) << '\n';
    // int main() true（GCC 的函数名带签名）

    // 2.2 关键差异：函数体内取 current() 得到的是"函数自己的位置"；
    //     默认实参处取 current() 得到的是"调用方的位置"——无宏日志的根基
    log("hello from main");                      // [L调用行号 int main()] hello from main

    // 2.3 三兄弟分工速览（对照）：
    //   constexpr 变量：编译期初始化 + 只读
    //   constinit 变量：编译期初始化 + 可写（防静态初始化顺序灾难）
    //   consteval 函数：只能编译期调用（constexpr 函数允许运行期调用）

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 编译期查表 + 运行期累加
    int total = 0;
    for (int v : kSquares) total += v;
    std::cout << total << '\n';                  // 14（0+1+4+9）

    // 3.2 日志多点调用：各自的行号自动正确
    log("first");                                // [L调用行号 int main()] first
    log("second");                               // [L调用行号 int main()] second

    // 3.3 实战心法（对照 ①②③）：
    //   想锁死"只在编译期"用 consteval；只是"可用编译期"用 constexpr
    //   全局可变状态用 constinit 声明（初始化时序由编译器背书）
    //   日志/断言要位置信息：默认实参 + source_location，宏可以退休
    return 0;
}
