// 第 49 课示例：std::print / std::println（C++23）—— CPP23 篇开篇
//
// 结构：① 原理验证 —— 一步到位的输出；println 两省心细节
//       ② 语法演示 —— 格式说明符全继承；指定流（stderr）
//       ③ 应用实战 —— 日志三件套 + 循环输出
//
// 本机（MSYS2 g++ 15.1.0）编译运行——注意链接 -lstdc++exp：
//   g++ -std=c++23 -Wall -Wextra 49_print.cpp -o run -lstdc++exp && ./run
// 先预测每行输出，再运行对照！

#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <print>

int main() {
    std::print("===== 1. PRINCIPLES =====\n");

    // 验证 1：格式化与输出合在一个接口；不保证无分配或单次系统调用
    std::print("{} {}\n", "vec", 3);              // vec 3

    // 验证 2：仍传入格式串，只是参数包为空；println 再追加换行
    std::println("auto newline");                 // auto newline
    std::println("");                             // 原始 C++23 的空行写法
    // 无参 println() 来自后续 P3142，本机在 C++23 模式下作为扩展提供

    // 验证 3：编译期校验照旧（注释保存编译错示例）
    // std::println("{:d}", "abc");   // 编译错：d 不接受字符串（同第 36 课）

    std::print("===== 2. SYNTAX =====\n");

    // 2.1 格式说明符全盘继承 format（第 36 课的技能直接复用）
    std::println("{:>8.2f}", 3.14159);            //     3.14
    std::println("{:*^12}", "mid");               // ****mid*****
    std::println("{0}+{0}={1}", 2, 4);            // 2+2=4

    // 2.2 指定流：错误/诊断走 stderr（重定向语义天然正确）
    std::println(stderr, "warn: disk {}%", 87);   // warn: disk 87%（stderr）

    // 2.3 宽度参数化（嵌套替换域照样可用）
    for (int w : {4, 8, 12})
        std::println("[{:>{}}]", "ab", w);
    // [  ab]
    // [      ab]
    // [          ab]

    std::print("===== 3. APPLICATION =====\n");

    // 3.1 日志三件套：对齐 + 分流（stdout/stderr）
    struct Item { const char* name; double price; };
    Item menu[]{{"latte", 4.5}, {"americano", 3.25}};
    for (const auto& it : menu)
        std::println("{:<12}{:>8.2f}", it.name, it.price);
    // latte           4.50
    // americano       3.25
    std::println(stderr, "{} items listed", 2);   // （stderr）

    // 3.2 数学小报表：说明符组合的实战
    for (int deg : {30, 45, 60})
        std::println("sin({:>3}) = {:.4f}", deg, std::sin(deg * 3.14159265 / 180));
    // sin( 30) = 0.5000
    // sin( 45) = 0.7071
    // sin( 60) = 0.8660

    // 3.3 实战心法（对照 ①②③）：
    //   需要格式化输出时可用 print，按项目需求选择其他 I/O 接口
    //   诊断流显式 stderr；println 不承诺每次刷新，行缓冲/终端路径仍可能刷新
    //   本机链接别忘 -lstdc++exp（Windows 终端支持在该库）
    return 0;
}
