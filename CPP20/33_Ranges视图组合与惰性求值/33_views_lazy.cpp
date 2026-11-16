// 第 33 课示例：Ranges 视图组合与惰性求值（C++20）
//
// 结构：① 原理验证 —— 构造零求值；take 短路；重迭代无缓存
//       ② 语法演示 —— filter/transform/take/drop 家族；闭包复用
//       ③ 应用实战 —— 无穷源 + 管道化业务查询
//
// 编译运行：
//   g++ -std=c++20 -Wall -Wextra 33_views_lazy.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <iostream>
#include <ranges>
#include <string>
#include <vector>

struct Person {
    std::string name;
    int age;
};

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    std::vector<int> v{1, 2, 3, 4, 5, 6};
    int calls = 0;

    // 验证 1：构造管道零求值 —— 食谱写好，菜没下锅
    // 计数仅用于观察本次遍历；工程代码不要依赖视图中副作用的精确次数。
    auto sq = v | std::views::transform([&calls](int x) {
        ++calls;
        return x * x;
    });
    std::cout << calls << '\n';              // 0

    // 验证 2：take 短路 —— 只拉动需要的元素
    for (int y : sq | std::views::take(3)) std::cout << y << ' ';
    std::cout << "| calls=" << calls << '\n';   // 1 4 9 | calls=3

    // 验证 3：重迭代无缓存 —— 第二趟全部重算
    for (int y : sq) std::cout << y << ' ';
    std::cout << "| calls=" << calls << '\n';   // 1 4 9 16 25 36 | calls=9

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 filter | transform 链：数据从左流到右
    auto even_x2 = v | std::views::filter([](int x) { return x % 2 == 0; })
                     | std::views::transform([](int x) { return x * 2; });
    for (int y : even_x2) std::cout << y << ' ';
    std::cout << '\n';                       // 4 8 12

    // 2.2 take / drop 切片；take_while / drop_while 按条件截头
    for (int y : v | std::views::drop(1) | std::views::take(2))
        std::cout << y << ' ';               // 2 3
    std::cout << '\n';
    for (int y : v | std::views::take_while([](int x) { return x < 4; }))
        std::cout << y << ' ';               // 1 2 3（首个不满足即停）
    std::cout << '\n';
    for (int y : v | std::views::drop_while([](int x) { return x < 4; }))
        std::cout << y << ' ';               // 4 5 6（去掉连续满足的头部）
    std::cout << '\n';

    // 2.3 闭包命名与复用：drop|take 组合成"去头取身"
    auto middle = std::views::drop(1) | std::views::take(2);
    for (int y : v | middle) std::cout << y << ' ';
    std::cout << '\n';                       // 2 3

    // 2.4 const filter_view 不可迭代（C++20 接口约束）：
    // const auto cev = v | std::views::filter(...);
    // for (int x : cev) {}   // 编译错：begin() 非 const
    // filter_view 对象需非 const；其他视图是否可 const 迭代由类型决定

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 管道化查询：大龄名单取前 2 个名字 —— 零中间容器
    std::vector<Person> team{{"ann", 30}, {"bob", 25},
                             {"cy", 35},  {"dee", 28}};
    auto seniors = team | std::views::filter([](const Person& p) { return p.age >= 28; })
                         | std::views::transform(&Person::name)
                         | std::views::take(2);
    for (const auto& n : seniors) std::cout << n << ' ';
    std::cout << '\n';                       // ann cy

    // 3.2 无穷源 + 惰性 = 只算需要的：前 4 个自然数的平方
    //     iota 生成 1,2,3,... 无穷序列（下一课细讲），take 一到即停
    for (int y : std::views::iota(1) | std::views::take(4))
        std::cout << y * y << ' ';
    std::cout << '\n';                       // 1 4 9 16

    // 3.3 实战心法（对照 ①②③）：
    //   一趟查询用管道，多趟消费先落地
    //   局部变量上的视图不逃逸（ref_view 只借不养）
    //   只读谓词推荐 const&（非 const 容器也允许不修改元素的 T& 谓词）
    //   副作用交给算法（eager），管道里只放纯函数
    return 0;
}
