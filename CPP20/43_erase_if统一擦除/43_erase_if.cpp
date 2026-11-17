// 第 43 课示例：std::erase / std::erase_if 统一擦除（C++20）
//
// 结构：① 原理验证 —— 一行顶"remove-erase 惯用法"；返回删除数
//       ② 语法演示 —— map/set/string 的统一接口
//       ③ 应用实战 —— 订单清理：值擦除、谓词擦除、保留序
//
// 编译运行：
//   g++ -std=c++20 -Wall -Wextra 43_erase_if.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

template <class C>
void show(const char* label, const C& c) {
    std::cout << label << ": ";
    for (const auto& x : c) std::cout << x << ' ';
    std::cout << '\n';
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：vector + 谓词 —— 旧世界两行的 remove-erase 惯用法
    std::vector<int> v{3, 1, 4, 1, 5, 9, 2, 6};
    // 旧写法（对照）：
    // v.erase(std::remove_if(v.begin(), v.end(),
    //                        [](int x) { return x % 2 == 1; }),
    //         v.end());
    auto removed = std::erase_if(v, [](int x) { return x % 2 == 1; });
    show("after", v);                             // after: 4 2 6
    std::cout << "removed=" << removed << ' ' << v.size() << '\n';
    // removed=5 3 —— 返回删除个数；**相对顺序保持**

    // 验证 2：值擦除 —— 全部匹配都删（不止第一个）
    std::vector<int> w{1, 2, 1, 3, 1};
    std::cout << std::erase(w, 1) << ' ';         // 3
    show("after", w);                             // after: 2 3

    // 验证 3：set 也能一行清（关联容器不再需要"边走边删"的迭代器舞步）
    std::set<int> tags{2, 3, 5, 7, 8};
    std::cout << std::erase_if(tags, [](int t) { return t % 2 == 0; })
              << '\n';                            // 2（删 2 与 8）
    show("tags", tags);                           // tags: 3 5 7

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 map：谓词收 (const auto& kv)——注意 pair<const K, V>
    std::map<std::string, int> stock{{"apple", 3}, {"pear", 0},
                                     {"fig", 0},  {"plum", 5}};
    std::cout << std::erase_if(stock,
                               [](const auto& kv) { return kv.second == 0; })
              << '\n';                            // 2（pear 与 fig）
    for (const auto& [name, n] : stock)           // 结构化绑定（第 11 课）
        std::cout << name << '=' << n << ' ';
    std::cout << '\n';                            // apple=3 plum=5

    // 2.2 string：按字符谓词/值擦除
    std::string s = "hello world";
    std::cout << std::erase_if(s, [](char c) { return c == 'l'; }) << ' ';
    std::cout << s << '\n';                       // 3 heo word
    std::cout << std::erase(s, 'o') << ' ' << s << '\n';   // 2 he wrd

    // 2.3 空容器与全删：返回 0 / size()，无 UB
    std::vector<int> empty;
    std::cout << std::erase_if(empty, [](int) { return true; }) << '\n';   // 0

    // 2.4 array 没有此接口（定长！注释保存）：
    // std::array<int, 3> arr{1, 2, 3};
    // std::erase_if(arr, [](int) { return true; });   // 编译错：无此重载

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 订单清理：清空已完成 + 删除指定编号
    struct Order { int id; std::string state; };
    std::vector<Order> orders{{1, "done"}, {2, "open"},
                              {3, "done"}, {4, "open"}};
    std::erase_if(orders,
                  [](const Order& o) { return o.state == "done"; });
    std::cout << orders.size() << ' ' << orders.front().id << ' '
              << orders.back().id << '\n';        // 2 2 4（原序保留）

    // 3.2 词表清洗：去空串 + 去重后统一小写化（去重借 set 一臂）
    std::vector<std::string> words{"Go", "", "rust", "", "go", "C"};
    std::erase_if(words, [](const std::string& w) { return w.empty(); });
    std::set<std::string> uniq(words.begin(), words.end());
    show("uniq", uniq);                           // uniq: C Go go rust

    // 3.3 实战心法（对照 ①②③）：
    //   按谓词清容器：erase_if 一行，忘掉 remove-erase 惯用法
    //   按值清：erase(v, value)（vector/string 全删，不是首删）
    //   关联容器清一批：erase_if 取代"迭代器 + erase 循环"
    //   返回值=删除个数——日志与计数直接用
    return 0;
}
