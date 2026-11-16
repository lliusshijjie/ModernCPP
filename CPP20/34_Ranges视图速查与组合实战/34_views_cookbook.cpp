// 第 34 课示例：Ranges 视图速查与组合实战（C++20）
//
// 结构：① 原理验证 —— iota 无穷源；reverse；keys/values
//       ② 语法演示 —— split 物化；join 拍平；counted/common 桥接
//       ③ 应用实战 —— 文本分词组合管道
//
// 编译运行：
//   g++ -std=c++20 -Wall -Wextra 34_views_cookbook.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <iostream>
#include <map>
#include <numeric>   // std::accumulate
#include <ranges>
#include <string>
#include <vector>

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：iota —— 有界数列 / 无穷数列（无穷必须配 take）
    for (int x : std::views::iota(1, 5)) std::cout << x << ' ';
    std::cout << '\n';                       // 1 2 3 4
    for (int x : std::views::iota(1) | std::views::take(4))
        std::cout << x << ' ';               // 1 2 3 4（无穷 + 截断）
    std::cout << '\n';

    // 验证 2：reverse —— 需要底层双向迭代
    std::vector<int> v{1, 2, 3};
    for (int x : v | std::views::reverse) std::cout << x << ' ';
    std::cout << '\n';                       // 3 2 1

    // 验证 3：keys / values —— map 的两列投影
    std::map<std::string, int> month_days{{"jan", 31}, {"feb", 28}};
    for (const auto& k : std::views::keys(month_days)) std::cout << k << ' ';
    std::cout << "| ";                       // feb jan（map 按键排序）
    for (int d : std::views::values(month_days)) std::cout << d << ' ';
    std::cout << '\n';                       // 28 31

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 split：产物是子区间（零拷贝），要 string 自己物化
    std::string csv = "a,bb,ccc";
    for (auto&& sub : csv | std::views::split(',')) {
        std::string token(sub.begin(), sub.end());   // 物化
        std::cout << token << ' ';
    }
    std::cout << '\n';                       // a bb ccc

    // 2.2 join：拍平一层嵌套
    std::vector<std::vector<int>> grid{{1, 2}, {3}, {4, 5}};
    for (int x : grid | std::views::join) std::cout << x << ' ';
    std::cout << '\n';                       // 1 2 3 4 5

    // 2.3 counted：迭代器 + 个数（老式接口转区间，不查越界）
    for (int x : std::views::counted(v.begin(), 2)) std::cout << x << ' ';
    std::cout << '\n';                       // 1 2

    // 2.4 common：哨兵归一，喂给 C++17 风格算法
    auto three = std::views::iota(1) | std::views::take(3)
               | std::views::common;
    std::cout << std::accumulate(three.begin(), three.end(), 0)
              << '\n';                       // 6

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 文本分词管道：切分 -> 物化 -> 筛选 -> 取长度
    std::string sentence = "the quick brown fox";
    auto words = sentence
        | std::views::split(' ')
        | std::views::transform([](auto&& sub) {
              return std::string(sub.begin(), sub.end());
          });
    for (const auto& w : words
              | std::views::filter([](const std::string& s) { return s.size() > 3; }))
        std::cout << w << ' ';
    std::cout << "| ";                       // quick brown |（the/fox 太短）
    for (std::size_t n : words | std::views::transform(&std::string::size))
        std::cout << n << ' ';
    std::cout << '\n';                       // 3 5 5 3

    // 3.2 map 两列 + 管道：天数合计（values 视图 + accumulate，配 common）
    auto days = std::views::values(month_days) | std::views::common;
    std::cout << std::accumulate(days.begin(), days.end(), 0)
              << '\n';                       // 59

    // 3.3 实战心法（对照 ①②③）：
    //   源（iota/split/join）管形态，filter/transform 管内容
    //   无穷源下游必有 take；split 产物要 string 先物化
    //   能力短板决定整条链；要排序先落地
    return 0;
}
