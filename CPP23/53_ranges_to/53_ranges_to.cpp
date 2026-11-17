// 第 53 课示例：ranges::to 与容器构造（C++23）
//
// 结构：① 原理验证 —— 管道终点直接落容器；元素类型推导/显式
//       ② 语法演示 —— 序列与关联容器；split 递归物化
//       ③ 应用实战 —— 词频统计全管道落地
//
// 本机（MSYS2 g++ 15.1.0）编译运行——注意链接 -lstdc++exp：
//   g++ -std=c++23 -Wall -Wextra 53_ranges_to.cpp -o run -lstdc++exp && ./run
// 先预测每行输出，再运行对照！

#include <algorithm>   // std::ranges::sort
#include <map>
#include <print>
#include <ranges>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

int main() {
    std::print("===== 1. PRINCIPLES =====\n");

    // 验证 1：管道终点直接落 vector——"先构容器再塞"的样板终结
    auto evens = std::views::iota(1, 10)
               | std::views::filter([](int x) { return x % 2 == 0; })
               | std::ranges::to<std::vector>();      // 推导 vector<int>
    for (int x : evens) std::print("{} ", x);
    std::println("| {}", evens.size());               // 2 4 6 8 | 4

    // 验证 2：显式元素类型（int 平方提升为 long）
    auto squares = std::views::iota(1, 5)
                 | std::views::transform([](int x) { return x * x; })
                 | std::ranges::to<std::vector<long>>();
    static_assert(std::is_same_v<decltype(squares), std::vector<long>>);
    std::println("{} {}", squares.size(), squares.back());   // 4 16

    // 旧世界对照（第 33 课之前的写法，注释保存）：
    //   std::vector<int> evens;
    //   for (int x = 1; x < 10; ++x) if (x % 2 == 0) evens.push_back(x);
    // 或：views 版 + back_inserter + for 循环逐个消费

    std::print("===== 2. SYNTAX =====\n");

    // 2.1 调用形态两版：管道右操作数 / 直接调用
    auto v1 = std::views::iota(0, 3) | std::ranges::to<std::vector>();
    auto v2 = std::ranges::to<std::vector>(std::views::iota(0, 3));
    std::println("{} {}", v1.size(), v2.size());      // 3 3

    // 2.2 关联容器：去重排序一条龙
    auto uniq = std::views::iota(1, 7)
              | std::views::transform([](int x) { return x % 3; })  // 1,2,0,1,2,0
              | std::ranges::to<std::set>();
    for (int x : uniq) std::print("{} ", x);
    std::println("| {}", uniq.size());                // 0 1 2 | 3

    // 2.3 pair 范围 -> map
    std::vector<std::pair<std::string, int>> vp{{"b", 2}, {"a", 1}};
    auto m = vp | std::ranges::to<std::map>();
    for (const auto& [k, val] : m) std::print("{}={} ", k, val);
    std::println("");                                 // a=1 b=2

    // 2.4 字符范围 -> string（注意 iota 半开：'a'..'e'）
    auto text = std::views::iota('a', 'f') | std::ranges::to<std::string>();
    std::println("{} {}", text, text.size());         // abcde 5

    // 2.5 第 34 课 split 物化的现代一行版
    std::string csv = "x,yy,zzz";
    auto tokens = csv | std::views::split(',')
                  | std::ranges::to<std::vector<std::string>>(); // 递归物化子区间
    std::println("{} {}", tokens.size(), tokens[1]);  // 3 yy

    std::print("===== 3. APPLICATION =====\n");

    // 3.1 词频统计：切词 -> 落地 -> 计数 -> 排序落地
    //    标准 ranges 管道不自动接受任意 lambda，需合适的范围适配器闭包——
    //    落地成容器后再用普通调用
    std::string words = "to be or not to be";
    auto tok = words | std::views::split(' ')
                | std::ranges::to<std::vector<std::string>>();
    std::map<std::string, int> freq;
    for (const auto& t : tok) ++freq[t];
    // 注意大坑：map | to<vector> 得到 pair<const string, int>——
    // 默认元素保留 const 键，不能直接 sort；显式指定普通 pair 即可转换
    auto by_freq = freq
                  | std::ranges::to<std::vector<std::pair<std::string, int>>>();
    std::ranges::sort(by_freq, [](const auto& x, const auto& y) {
        return x.second != y.second ? x.second > y.second   // 频次降序
                                    : x.first < y.first;    // 同频字母序（保确定）
    });
    for (const auto& [w, n] : by_freq) std::print("{}:{} ", w, n);
    std::println("");                                 // be:2 to:2 not:1 or:1

    // 3.2 实战心法（对照 ①②③）：
    //   管道末端要容器：to 一发；元素类型拿不准就显式写
    //   多趟消费（排序/随机访问）先落地；一趟遍历保持视图
    //   to 的适用路径会自动 reserve；按实际构造路径/范围大小能力测量分配
    return 0;
}
