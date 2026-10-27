// 第 08 课示例：chrono 用户自定义字面量与时间单位（C++14）
//
// 结构：① 原理验证 —— 单位在类型里；混算取细；变粗必须显式截断
//       ② 语法演示 —— 六后缀全家福；跨单位比较；sleep_for 吃字面量
//       ③ 应用实战 —— 自定义长度单位字面量；超时语义自文档化
//
// 编译运行：
//   g++ -std=c++14 -Wall -Wextra 08_chrono_literals.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <chrono>
#include <iostream>
#include <thread>
#include <type_traits>

using namespace std::chrono_literals;   // 演示用文件级；工程建议函数级

// ③ 自定义字面量：把 chrono 的"单位进类型"模式抄到长度领域
struct Meters {
    double v;
};
constexpr Meters operator+(Meters a, Meters b) { return Meters{a.v + b.v}; }
constexpr Meters operator""_km(unsigned long long km) {   // 整数字面量走这个
    return Meters{static_cast<double>(km) * 1000.0};   // 折算成米
}
constexpr Meters operator""_km(long double km) {          // 浮点字面量走这个
    return Meters{static_cast<double>(km) * 1000.0};
}
constexpr Meters operator""_m(unsigned long long m) {
    return Meters{static_cast<double>(m)};
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：不同后缀是不同类型 —— 单位编码在类型里
    static_assert(std::is_same<decltype(2h), std::chrono::hours>::value, "");
    static_assert(std::is_same<decltype(30min), std::chrono::minutes>::value, "");
    static_assert(std::is_same<decltype(90s), std::chrono::seconds>::value, "");

    // 验证 2：混单位运算自动换算到更细的一方（无损方向）
    auto total = 2h + 30min;   // common_type = minutes
    static_assert(std::is_same<decltype(total), std::chrono::minutes>::value, "");
    std::cout << total.count() << '\n';      // 150

    // 验证 3：变粗必须显式 duration_cast —— 截断发生在这一步
    auto coarse = std::chrono::duration_cast<std::chrono::hours>(90min);
    std::cout << coarse.count() << '\n';     // 1（向零截断，不是 1.5）

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 六后缀全家福：UDL 与整个字面量 token 绑定，
    //     紧跟 .count() 会被当成后缀的一部分 —— 先加括号再取成员
    std::cout << (100ms).count() << ' ' << (1ns).count() << '\n';   // 100 1

    // 2.2 浮点字面量 → 浮点 rep 的 duration（与整数版是不同类型！）
    auto half = 1.5h;
    static_assert(std::is_floating_point<typename decltype(half)::rep>::value, "");
    static_assert(decltype(half)::period::num == 3600 &&
                  decltype(half)::period::den == 1, "");
    std::cout << half.count() << '\n';       // 1.5

    // 2.3 跨单位相等比较：编译器按 ratio 自动换算
    std::cout << (90s == 1min + 30s) << '\n';   // true
    std::cout << (2h == 120min) << '\n';        // true

    // 2.4 超时 API 直接吃字面量（对比旧写法 milliseconds(100)）
    auto t0 = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(100ms);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - t0).count();
    std::cout << (elapsed >= 100) << '\n';   // true

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 自定义长度字面量：2_km + 500_m 全部折算成米
    auto race = 2_km + 500_m;
    std::cout << race.v << '\n';             // 2500

    // 3.2 语义自文档化：字面量自带单位，注释不会过期
    static_assert(std::is_same<decltype(60s), std::chrono::seconds>::value, "");
    std::cout << std::is_same<decltype(60s), std::chrono::seconds>::value << '\n';   // true

    return 0;
}
