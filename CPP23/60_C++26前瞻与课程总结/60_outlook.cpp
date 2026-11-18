// 第 60 课示例：C++26 前瞻与课程总结——收官课
//
// 结构：① 原理验证 —— 课程知识地图自检（60 课知识点串联）
//       ② 语法演示 —— 四个标准的能力速览（每项一行）
//       ③ 应用实战 —— C++26 已可用特性的"预告片"（C++26 部分见
//                     60_cpp26_glimpse.cpp，用 -std=c++26 编译）
//
// 编译运行（本机 g++ 15.1.0，本课用 C++23 + 链接 -lstdc++exp）：
//   g++ -std=c++23 -Wall -Wextra 60_outlook.cpp -o run -lstdc++exp && ./run
// 先预测每行输出，再运行对照！

#include <cstddef>
#include <expected>
#include <generator>
#include <limits>
#include <print>
#include <ranges>
#include <string>
#include <vector>

// ① 串联多个课程知识点的"毕业生自检"：
// generator（54）+ 视图（33/34）+ expected（50）+
// print（49）+ 范围格式化（59）+ ranges::to（53）
// 只产出 int 可表示的正整数平方；除法检查避免乘法先溢出。
constexpr int max_square_root() {
    int i = 1;
    while (i < std::numeric_limits<int>::max() &&
           i + 1 <= std::numeric_limits<int>::max() / (i + 1)) ++i;
    return i;
}
constexpr std::size_t max_even_squares =
    static_cast<std::size_t>(max_square_root() / 2);

std::generator<int> squares() {                 // 第 54 课
    for (int i = 1; i <= std::numeric_limits<int>::max() / i; ++i)
        co_yield i * i;
}

std::expected<std::vector<int>, std::string> first_even_squares(std::size_t n) {
    if (n == 0) return std::unexpected("n must be positive");   // 第 50 课
    if (n > max_even_squares)
        return std::unexpected("n exceeds representable even squares");
    // 已验证 n 很小，显式转换到该 range 的有符号差值类型。
    auto picked = squares()
        | std::views::filter([](int x) { return x % 2 == 0; })  // 第 33 课
        | std::views::take(static_cast<std::ptrdiff_t>(n))       // 第 33 课
        | std::ranges::to<std::vector>();                       // 第 53 课
    return picked;
}

int main() {
    std::print("===== 1. PRINCIPLES =====\n");

    // 1.1 毕业生自检：一条管道串起六课
    auto r = first_even_squares(3);
    std::print("{} {}\n", r.has_value() ? "ok" : "err",
                r.value_or(std::vector<int>{-1}));
    // ok [4, 16, 36]

    auto bad = first_even_squares(0);
    std::print("{} {}\n", bad.has_value() ? "ok" : "err", bad.error());
    // err n must be positive

    std::print("===== 2. SYNTAX =====\n");

    // 2.1 四个标准的能力速览（各一行）
    std::print("C++14: {} / C++17: {} / C++20: {} / C++23: {}\n",
        "auto lambda, constexpr+, integer_sequence",   // 01~10
        "structured bindings, if constexpr, string_view",  // 11~30
        "concepts, ranges, <=>, format, jthread, coroutine",  // 31~48
        "print, expected, generator, flat_map");       // 49~59

    // 2.2 进化主线（一句话总结）
    std::print("main line: {} -> {} -> {} -> {}\n",
        "manual types", "generic by deduction",
        "constrained generics", "ergonomics & safety");

    std::print("===== 3. APPLICATION =====\n");

    // 3.1 C++26 前瞻（本机 g++ 15 已实测可用的两项，见姊妹文件）
    std::print("try -std=c++26 60_cpp26_glimpse.cpp -> pack indexing, "
               "delete(reason)\n");

    auto too_many = first_even_squares(std::numeric_limits<std::size_t>::max());
    std::print("oversized accepted={}\n", too_many.has_value()); // false

    // 3.2 收官心法：
    //   语言层：类型越来越"会说人话"（concept/格式化/错误值）
    //   库层：管道与视图接管数据流（ranges/generator/to）
    //   工具层：可预先完成的计算从运行期移到编译期（constexpr 容器/consteval）
    //   学习法：每课的"先预测再运行"就是 C++ 的科学实验方法
    std::print("60/60 lessons done. keep predicting, keep running.\n");
    return 0;
}
