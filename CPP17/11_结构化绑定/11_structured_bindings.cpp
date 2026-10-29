// 第 11 课示例：结构化绑定 structured bindings（C++17）—— CPP17 篇开篇
//
// 结构：① 原理验证 —— 绑定名是隐藏对象的成员别名（引用式 vs 拷贝式）
//       ② 语法演示 —— 数组/pair/tuple 三种目标 + map 遍历
//       ③ 应用实战 —— insert 结果分解；词频统计
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 11_structured_bindings.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：auto& 形式 —— 绑定名是原对象成员的别名
    struct Point { int x; int y; };
    Point p{10, 20};
    auto& [px, py] = p;
    // 注意标准细节：decltype(绑定名) 给成员的"声明类型"（struct 情形去引用）
    // —— 虽然语义上 px 仍是 p.x 的别名（下一行就是证据）
    static_assert(std::is_same_v<decltype(px), int>, "struct 情形：声明类型");
    px += 1;
    py += 2;
    std::cout << p.x << ' ' << p.y << '\n';    // 11 22 —— 直接改到了 p

    // 验证 2：auto 形式 —— 隐藏对象是拷贝
    auto [cx, cy] = p;
    cx = 0;
    std::cout << cx << ' ' << cy << '\n';      // 0 22 —— 改的是拷贝
    std::cout << p.x << '\n';                  // 11 —— 原对象不受影响

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 C 风格数组
    int coords[3] = {1, 2, 3};
    auto [a1, a2, a3] = coords;
    std::cout << a1 << ' ' << a2 << ' ' << a3 << '\n';   // 1 2 3

    // 2.2 pair（注意：实参必须是具名变量，见陷阱 1）
    int lo_int = 3, hi_int = 7;
    auto [lo, hi] = std::minmax(lo_int, hi_int);
    std::cout << lo << ' ' << hi << '\n';      // 3 7

    // 2.3 tuple
    auto [i, d, s] = std::make_tuple(1, 2.5, std::string("sb"));
    std::cout << i << ' ' << d << ' ' << s << '\n';      // 1 2.5 sb

    // 2.4 map 遍历 —— 结构化绑定的头号用例
    std::map<std::string, int> stock{{"pen", 3}, {"book", 7}};
    for (const auto& [name, count] : stock) {
        std::cout << name << '=' << count << '\n';       // book=7 / pen=3
    }

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 分解 insert 的返回值 pair<iterator, bool>
    std::map<std::string, int> m{{"alpha", 1}};
    auto [it, inserted] = m.insert({"alpha", 100});
    std::cout << inserted << ' ' << it->second << '\n';  // false 1 —— 键已存在
    auto [it2, inserted2] = m.insert({"beta", 2});
    std::cout << inserted2 << ' ' << it2->second << '\n';// true 2 —— 新插入

    // 3.2 词频统计：改与读都用绑定名
    std::vector<std::string> words{"go", "go", "cpp"};
    std::map<std::string, int> cnt;
    for (const auto& w : words) ++cnt[w];
    for (const auto& [w, n] : cnt) {
        std::cout << w << ':' << n << ' ';
    }
    std::cout << '\n';                         // cpp:1 go:2

    return 0;
}
