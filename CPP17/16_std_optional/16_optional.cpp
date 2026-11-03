// 第 16 课示例：std::optional（C++17）
//
// 结构：① 原理验证 —— T 不要求默认构造；emplace/reset/赋值三态操作
//       ② 语法演示 —— 三种取值方式；CTAD；比较语义
//       ③ 应用实战 —— 查找型接口；value_or 兜底
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 16_optional.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <type_traits>

// ① T 不要求默认构造 —— optional 照样能装
struct NoDefault {
    int v;
    explicit NoDefault(int v) : v(v) {}
};
static_assert(!std::is_default_constructible_v<NoDefault>, "故意的");

// ③ 查找型接口：取代指针 / 魔法值 / 异常
std::optional<int> find_age(const std::map<std::string, int>& m,
                            const std::string& k) {
    auto it = m.find(k);
    if (it == m.end()) return std::nullopt;   // 空态用 nullopt 表达
    return it->second;
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：空态创建 + 原地构造（NoDefault 无法先默认构造再赋值）
    std::optional<NoDefault> nd;              // 空的 optional<NoDefault>
    std::cout << nd.has_value() << '\n';      // false
    nd.emplace(7);                            // 原地构造，无需 T 默认构造
    std::cout << nd->v << '\n';               // 7

    // 验证 2：reset 回空态；赋值即置位
    std::optional<int> o = 42;
    o.reset();
    std::cout << o.has_value() << '\n';       // false
    o = 9;
    std::cout << o.value() << '\n';           // 9

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 三种取值：*（先判） / value()（空则抛） / value_or（安全兜底）
    std::optional<int> empty;
    // std::cout << *empty;      // 空时 UB —— 快但必须先判
    // empty.value();            // 空时抛 bad_optional_access
    std::cout << empty.value_or(-1) << '\n';  // -1

    // 2.2 CTAD 推导（第 15 课的应用）
    auto od = std::optional{3.14};
    static_assert(std::is_same_v<decltype(od), std::optional<double>>, "");
    std::cout << od.value() << '\n';          // 3.14

    // 2.3 比较语义：空 < 有值；nullopt 表达空
    std::optional<int> a;                     // 空
    std::optional<int> b = 5;
    std::cout << (a < b) << '\n';             // true
    std::cout << (a == std::nullopt) << '\n'; // true

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 查找型接口：两条路都自然（if-init + optional 上下文转 bool）
    std::map<std::string, int> ages{{"Tom", 18}};
    if (auto age = find_age(ages, "Tom")) {
        std::cout << "Tom=" << *age << '\n';          // Tom=18
    }
    if (auto age = find_age(ages, "Jerry")) {
        std::cout << "Jerry=" << *age << '\n';        // 走不到
    } else {
        std::cout << "Jerry unknown\n";               // Jerry unknown
    }

    // 3.2 value_or：一行表达"有就用，没有给默认"
    int port = find_age(ages, "port").value_or(8080);
    std::cout << port << '\n';                        // 8080

    return 0;
}
