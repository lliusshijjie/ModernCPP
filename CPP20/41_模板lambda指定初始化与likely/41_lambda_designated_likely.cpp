// 第 41 课示例：模板 lambda、指定初始化器与 [[likely]]（C++20）
//
// 结构：① 原理验证 —— 指定初始化的边界；模板 lambda 拿数组长度
//       ② 语法演示 —— 嵌套指定；约束模板 lambda；[[likely]] 位置
//       ③ 应用实战 —— 配置构造 + 变参折叠 + 分支标注
//
// 编译运行：
//   g++ -std=c++20 -Wall -Wextra 41_lambda_designated_likely.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <concepts>
#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

struct Window {                       // 聚合：无用户构造函数（02A）
    int x = 0;                        // 默认成员初值
    int y = 0;
    std::string title = "untitled";
    bool fullscreen = false;
};

struct Theme {
    std::string fg;
    Window login;                     // 嵌套聚合
};

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：指定初始化——指名道姓、可跳过、默认值生效
    Window w{.y = 30, .title = "editor"};
    std::cout << w.x << ' ' << w.y << ' ' << w.title << ' '
              << w.fullscreen << '\n';            // 0 30 editor false

    // 四条边界（全部实测编译错，注释保存）：
    // Window e1{.title = "x", .y = 1};   // 乱序：指定符必须按声明序
    // Window e2{.x = 1, 30};             // 混用：指定与位置不能混
    // struct Arr{int a[2];};
    // Arr e3{.a[0] = 1};                 // 数组指示符不存在（C99 专属）
    // struct Ctor{Ctor(){} int a;};
    // Ctor e4{.a = 1};                   // 非聚合：有用户构造即出局

    // 验证 2：模板 lambda——数组引用不退化，长度进类型
    auto arr_len = []<class T, std::size_t N>(T (&)[N]) { return N; };
    int a[]{1, 2, 3, 4};
    std::cout << arr_len(a) << '\n';              // 4

    // 验证 3：C++14 的 auto& 同样能保留数组；显式形参表让 T/N 更易命名
    auto old_len = [](const auto& r) { return sizeof(r) / sizeof(r[0]); };
    // const auto& 不退化，decltype(r) 也能配合 traits 取得数组/元素类型：
    std::cout << old_len(a) << '\n';              // 4（可用 decltype/traits；显式 T、N 更直接）

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 嵌套指定初始化
    Theme t{.fg = "white", .login{.x = 1, .y = 2}};
    std::cout << t.fg << ' ' << t.login.x << t.login.y << ' '
              << t.login.title << '\n';           // white 12 untitled

    // 2.2 模板形参加 concept 约束（第 31 课）
    auto twice = []<std::integral T>(T v) { return v * 2; };
    std::cout << twice(21) << ' ' << twice('a') << '\n';   // 42 194
    // twice(1.5);   // 编译错：double 不满足 std::integral

    // 2.3 模板 lambda 收容器：元素类型 T 直接可用
    auto describe = []<class T>(const std::vector<T>& v) {
        return std::to_string(v.size()) + " x " + (std::is_same_v<T, int> ? "int" : "T");
    };
    std::cout << describe(std::vector<int>{1, 2, 3}) << '\n';   // 3 x int

    // 2.4 [[likely]] / [[unlikely]]：标在分支语句前
    int score = 85;
    if (score >= 60) [[likely]] {
        std::cout << "pass\n";                    // pass
    } else [[unlikely]] {
        std::cout << "fail\n";
    }

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 配置对象：只写关心的字段
    Window popup{.title = "about", .fullscreen = true};
    std::cout << popup.title << ' ' << popup.fullscreen << ' '
              << popup.x << popup.y << '\n';      // about true 00

    // 3.2 变参模板 lambda + 折叠（第 14 课）+ 约束
    auto sum_all = []<std::integral... Ts>(Ts... vs) {
        return (vs + ... + 0);
    };
    std::cout << sum_all(1, 2, 3, 4) << '\n';     // 10

    // 3.3 实战心法（对照 ①②③）：
    //   多字段聚合一律指定式——字段名即文档
    //   要元信息（长度/元素类型）就上模板形参表，别再 auto 猜
    //   [[likely]] 只标测过的路径，无需强制成对标记
    return 0;
}
