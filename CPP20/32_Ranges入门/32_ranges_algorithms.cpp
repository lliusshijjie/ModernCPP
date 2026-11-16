// 第 32 课示例：Ranges 入门：新的算法与管道（C++20）
//
// 结构：① 原理验证 —— 容器直传；投影排序；悬垂防御 dangling
//       ② 语法演示 —— 迭代器+哨兵重载；size/ssize；比较器与投影组合
//       ③ 应用实战 —— 结构体集合的全投影化操作
//
// 编译运行：
//   g++ -std=c++20 -Wall -Wextra 32_ranges_algorithms.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <algorithm>   // std::ranges::sort/find/count_if/max_element/transform
#include <functional>  // std::greater
#include <iostream>
#include <iterator>    // std::back_inserter
#include <ranges>      // std::ranges::dangling
#include <string>
#include <type_traits>
#include <vector>

struct Person {
    std::string name;
    int age;
};

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：容器直传 —— begin/end 时代结束
    std::vector<int> v{5, 3, 8, 1, 9, 2};
    std::ranges::sort(v);                    // = std::sort(v.begin(), v.end())
    for (int x : v) std::cout << x << ' ';
    std::cout << '\n';                       // 1 2 3 5 8 9

    // 验证 2：投影 —— 成员指针就是可调用对象，比较前先"取字段"
    std::vector<Person> ps{{"ann", 30}, {"bob", 25}, {"cy", 35}};
    std::ranges::sort(ps, {}, &Person::age); // {} = 默认比较 ranges::less
    for (const auto& p : ps) std::cout << p.name << ' ';
    std::cout << '\n';                       // bob ann cy（按 age 升序换的是整个元素）

    // 验证 3：悬垂防御 —— 右值容器 find 返回 dangling，UB 降级为编译错
    auto d = std::ranges::find(std::vector<int>{1, 2, 3}, 2);
    std::cout << std::is_same_v<decltype(d), std::ranges::dangling>
              << '\n';                       // true
    // *d;  // 编译错：dangling 无 operator* —— 旧世界这里是悬垂迭代器
    auto it = std::ranges::find(v, 8);       // 左值容器：真迭代器
    std::cout << (it != v.end()) << ' ' << *it << '\n';   // true 8

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 迭代器 + 哨兵重载仍然存在（哨兵类型可不同于迭代器，第 33 课细讲）
    auto it2 = std::ranges::find(v.begin(), v.end(), 5);
    std::cout << *it2 << '\n';               // 5

    // 2.2 size（vector 返回无符号类型、摊销 O(1)）与 ssize（有符号）
    std::cout << std::ranges::size(v) << ' '
              << std::ranges::ssize(v) << ' '
              << *std::ranges::data(v) << '\n';   // 6 6 1

    // 2.3 投影可以是任意可调用：lambda 按"个位数"排序
    std::vector<int> w{17, 23, 8, 41, 5};
    std::ranges::sort(w, {}, [](int x) { return x % 10; });
    for (int x : w) std::cout << x << ' ';
    std::cout << '\n';                       // 41 23 5 17 8（个位数 1 3 5 7 8）

    // 2.4 比较器 + 投影组合：greater 搭配投影
    std::ranges::sort(w, std::greater{}, [](int x) { return x % 10; });
    for (int x : w) std::cout << x << ' ';
    std::cout << '\n';                       // 8 17 5 23 41（个位数降序）

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 结构体集合全投影化：排序/查找/计数/提取，零比较 lambda
    std::vector<Person> team{{"ann", 30}, {"bob", 25}, {"cy", 35}};
    std::ranges::sort(team, {}, &Person::age);          // 按 age 升序
    auto p = std::ranges::find(team, "ann", &Person::name);   // 按 name 查
    std::cout << p->age << '\n';             // 30
    std::cout << std::ranges::count_if(
                     team, [](int a) { return a >= 30; }, &Person::age)
              << '\n';                       // 2（ann、cy）
    auto oldest = std::ranges::max_element(team, {}, &Person::age);
    std::cout << oldest->name << '\n';       // cy

    // 3.2 transform 的操作函数：成员指针直接提取字段（此参数不是投影）
    std::vector<std::string> names;
    std::ranges::transform(team, std::back_inserter(names), &Person::name);
    for (const auto& s : names) std::cout << s << ' ';
    std::cout << '\n';                       // bob ann cy

    // 3.3 实战心法（对照 ①②③）：
    //   传容器不传迭代器对     —— ranges::sort(v)
    //   按字段先想投影         —— (v, {}, &T::field)
    //   临时区间先落地再查询   —— 否则拿回 dangling
    //   立即求值               —— 惰性流水线等下一课 views
    return 0;
}
