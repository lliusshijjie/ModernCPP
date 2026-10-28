// 补充 05A：透明比较器与异构查找（C++14）
// ① 原理验证 -> ② 语法演示 -> ③ 应用实战；输出仅使用 ASCII。
// 从 ModernCPP 目录编译：
// g++ -std=c++14 -Wall -Wextra -Wpedantic "CPP14/补充知识点/05_补充A_透明比较器与异构查找/05a_heterogeneous_lookup.cpp" -o run

#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <string>

struct Product {
    int id;
    std::string name;
};

struct ById {
    using is_transparent = void;

    bool operator()(const Product& a, const Product& b) const {
        return a.id < b.id;
    }
    bool operator()(const Product& a, int id) const {
        return a.id < id;
    }
    bool operator()(int id, const Product& b) const {
        return id < b.id;
    }
};

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";
    const std::less<int> fixed;
    const std::less<> transparent;
    std::cout << fixed(2, 2.5) << ' ' << transparent(2, 2.5) << '\n'; // false true

    std::cout << "===== 2. SYNTAX =====\n";
    const std::map<std::string, int, std::less<>> scores{
        {"alice", 90}, {"bob", 80}
    };
    const char* query = "alice";
    const auto found = scores.find(query);
    if (found != scores.end()) {
        std::cout << found->first << ' ' << found->second << '\n'; // alice 90
    }
    std::cout << scores.count("nobody") << '\n';                  // 0

    const std::set<std::string, std::less<>> names{"alice", "bob"};
    const auto lower = names.lower_bound("b");
    if (lower != names.end()) {
        std::cout << *lower << '\n';                              // bob
    }

    std::cout << "===== 3. APPLICATION =====\n";
    const std::set<Product, ById> products{
        {30, "gamma"}, {10, "alpha"}, {20, "beta"}
    };
    const auto product = products.find(20); // 不需要先创建 Product。
    if (product != products.end()) {
        std::cout << product->id << ' ' << product->name << '\n'; // 20 beta
    }
    const auto next = products.lower_bound(15);
    if (next != products.end()) {
        std::cout << next->id << '\n';                            // 20
    }
    std::cout << (products.find(99) == products.end()) << '\n';   // true
}
