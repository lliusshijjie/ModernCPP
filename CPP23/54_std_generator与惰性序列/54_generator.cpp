// 第 54 课示例：std::generator 与惰性序列（C++23）
//
// 结构：① 原理验证 —— 六件套退役；生成器接视图管道
//       ② 语法演示 —— 按值/按引用产出；有限与无穷
//       ③ 应用实战 —— 递归树遍历（elements_of 委托）
//
// 本机（MSYS2 g++ 15.1.0）编译运行——注意链接 -lstdc++exp：
//   g++ -std=c++23 -Wall -Wextra 54_generator.cpp -o run -lstdc++exp && ./run
// 先预测每行输出，再运行对照！

#include <algorithm>   // std::ranges::count_if
#include <generator>
#include <memory>
#include <print>
#include <ranges>
#include <string>
#include <vector>

// ① 无穷序列：状态（a/b）住在协程帧里——视图写不出的"有状态源"
std::generator<int> fib() {
    int a = 0, b = 1;
    while (true) {
        co_yield a;
        auto next = a + b;
        a = b;
        b = next;
    }
}

// ② 有限序列：产出行程编码解码（状态：当前字符剩余计数）
std::generator<char> rle_decode(const std::string& s) {
    int count = 0;
    char ch = 0;
    for (char c : s) {
        if (c >= '0' && c <= '9') {
            count = count * 10 + (c - '0');
        } else {
            ch = c;
            for (int i = 0; i < count; ++i) co_yield ch;   // 按需逐个吐出
            count = 0;
        }
    }
}

// ③ 递归树：elements_of 委托子生成器——无显式栈的深度优先
struct Node {
    int value;
    std::vector<std::unique_ptr<Node>> kids;
};

std::generator<const int&> walk(const Node& n) {
    co_yield n.value;                                   // 引用产出：零拷贝
    for (const auto& k : n.kids)
        co_yield std::ranges::elements_of(walk(*k));    // 委托递归
}

int main() {
    std::print("===== 1. PRINCIPLES =====\n");

    // 验证 1：generator 是 view——直接接视图管道（第 33 课技能复用）
    for (int x : fib() | std::views::take(8))
        std::print("{} ", x);
    std::println("| fib-take8");                        // 0 1 1 2 3 5 8 13

    // 验证 2：右端还能继续加工（无穷源 + 惰性短路）
    for (int x : fib() | std::views::filter([](int v) { return v % 2 == 0; })
                        | std::views::take(3))
        std::print("{} ", x);
    std::println("| first3-evens");                     // 0 2 8

    // 验证 3：构造零求值——不迭代就不执行（第 33 课惰性同款）
    auto unused = fib();                                // 什么都没发生
    (void)unused;

    std::print("===== 2. SYNTAX =====\n");

    // 2.1 有限生成器：行程解码（"3a2b" -> aaabb）
    for (char c : rle_decode("3a2b")) std::print("{}", c);
    std::println("");                                   // aaabb

    // 2.2 单趟性：耗尽后再迭代是【未定义行为】——不是空输出！
    //    libstdc++ 实测：二次 range-for 触发断言中止
    //    （Assertion '!this->_M_began' failed）——注释保存，不实际运行
    auto g = rle_decode("2x");
    for (char c : g) std::print("{}", c);               // xx
    std::println("| single pass only");
    // for (char c : g) {}   // UB：断言中止（标准：begin() 前置条件已破坏）
    // 想再消费：重新调用 rle_decode(...) 或落地成容器（见 2.3）

    // 2.3 落地容器后再消费：ranges::to 接力（第 53 课）
    auto material = rle_decode("2y") | std::ranges::to<std::string>();
    std::println("{} {}", material, material.size());   // yy 2

    std::print("===== 3. APPLICATION =====\n");

    // 3.1 递归树遍历：根 - 子树依次展开（深度优先）
    Node root{1, {}};
    root.kids.push_back(std::make_unique<Node>(2));
    root.kids.push_back(std::make_unique<Node>(3));
    root.kids[0]->kids.push_back(std::make_unique<Node>(4));
    //        1
    //       / (2)(3)
    //      2   3
    //      |
    //      4
    for (int v : walk(root)) std::print("{} ", v);
    std::println("| dfs");                              // 1 2 4 3

    // 3.2 生成器管道化报表：树的偶数值计数
    auto even_cnt = [](const Node& n) {
        return static_cast<std::size_t>(
            std::ranges::count_if(walk(n), [](int v) { return v % 2 == 0; }));
    };
    std::println("{}", even_cnt(root));                 // 2（2 与 4）

    // 3.3 实战心法（对照 ①②③）：
    //   有状态源用 generator、无状态加工用视图、落地用 ranges::to
    //   递归遍历 elements_of；大对象 generator<const T&>
    //   单趟！多趟先落地；帧内不持锁跨 yield
    return 0;
}
