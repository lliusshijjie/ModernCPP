// 第 17 课示例：std::variant 与 std::visit（C++17）
//
// 结构：① 原理验证 —— tagged union 的安全标签；三种取值方式
//       ② 语法演示 —— visit 分派；overloaded 惯用法
//       ③ 应用实战 —— 事件系统；轻量状态机
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 17_variant_visit.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <iostream>
#include <string>
#include <variant>
#include <vector>

// ② overloaded 惯用法：三课知识的合体
//    using... 包引入 + CTAD 显式指引（第 14/15 课）
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// ③ 事件系统：封闭的消息集合
struct Click { int x, y; };
struct KeyPress { char key; };
struct Quit {};

using Event = std::variant<Click, KeyPress, Quit>;

// ③ 轻量状态机：每个状态携带自己的数据
struct Idle { int counter = 0; };
struct Running { int progress = 0; };
struct Done { std::string report; };
using State = std::variant<Idle, Running, Done>;

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：编译器维护标签 —— index() 与 holds_alternative 一致
    std::variant<int, std::string> v = 42;
    std::cout << v.index() << ' '
              << std::holds_alternative<int>(v) << '\n';      // 0 true

    // 验证 2：赋值即切换备选（旧成员自动析构）
    v = std::string("switched");
    std::cout << v.index() << ' '
              << std::holds_alternative<std::string>(v) << '\n';  // 1 true

    // 验证 3：三种取值 —— get<T>(抛) / get<I>(抛) / get_if(不抛，给 nullptr)
    std::cout << std::get<std::string>(v) << '\n';            // switched
    if (auto p = std::get_if<int>(&v)) {
        std::cout << *p << '\n';                              // 走不到
    } else {
        std::cout << "not int\n";                             // not int
    }

    // 验证 4：默认构造 = 第一个备选（variant 没有 optional 那种空态）
    std::variant<int, std::string> fresh;
    std::cout << fresh.index() << ' ' << fresh.valueless_by_exception() << '\n';  // 0 false

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 visit + 单个泛型 lambda（if constexpr 按实际类型分支，第 13 课）
    auto describe = [](auto&& x) {
        using X = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<X, int>) {
            return std::string("int:");
        } else {
            return std::string("str:");
        }
    };
    std::variant<int, std::string> w = 7;
    std::cout << std::visit(describe, w) << '\n';             // int:
    w = std::string("s");
    std::cout << std::visit(describe, w) << '\n';             // str:

    // 2.2 overloaded：每个备选一个 lambda 分支 —— variant 的标准姿势
    auto len = overloaded{
        [](int n) { return n; },
        [](const std::string& s) { return static_cast<int>(s.size()); }
    };
    std::cout << len(42) << ' ' << len(std::string("abcd")) << '\n';   // 42 4

    // 2.3 visit 对 overloaded 的分派：穷尽所有备选，漏一个编译不过
    std::visit(overloaded{
        [](int n) { std::cout << "int " << n << '\n'; },
        [](const std::string& s) { std::cout << "str " << s << '\n'; }
    }, w);                                                    // str s

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 事件系统：visit(overloaded{...}) 穷尽处理
    std::vector<Event> queue{Click{3, 4}, KeyPress{'x'}, Quit{}};
    for (const auto& e : queue) {
        std::visit(overloaded{
            [](const Click& c) { std::cout << "click " << c.x << ',' << c.y << '\n'; },
            [](const KeyPress& k) { std::cout << "key " << k.key << '\n'; },
            [](Quit) { std::cout << "quit\n"; }
        }, e);
    }
    // click 3,4 / key x / quit

    // 3.2 状态机：状态转换即"返回新的备选"，visit 读当前状态
    State st = Idle{};
    std::visit(overloaded{
        [](Idle& i) { std::cout << "idle " << i.counter << '\n'; },
        [](Running& r) { std::cout << "run " << r.progress << '\n'; },
        [](Done& d) { std::cout << "done " << d.report << '\n'; }
    }, st);                                                   // idle 0
    st = Running{55};
    std::visit(overloaded{
        [](Idle&) { std::cout << "idle\n"; },
        [](Running& r) { std::cout << "run " << r.progress << '\n'; },
        [](Done&) { std::cout << "done\n"; }
    }, st);                                                   // run 55

    return 0;
}
