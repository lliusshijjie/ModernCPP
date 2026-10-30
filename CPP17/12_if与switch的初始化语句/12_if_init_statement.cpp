// 第 12 课示例：if 与 switch 的初始化语句（C++17）
//
// 结构：① 原理验证 —— 作用域覆盖两个分支；init 恰好求值一次
//       ② 语法演示 —— 结构化绑定组合；switch 的 init；表达式 init
//       ③ 应用实战 —— 查表双分支；错误码风格雏形
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 12_if_init_statement.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <cctype>
#include <iostream>
#include <map>
#include <string>

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：init 变量在两个分支都可见 —— else 也能用它
    std::map<std::string, int> m{{"cpp", 2011}, {"mod", 26}};
    if (auto it = m.find("cpp"); it != m.end()) {
        std::cout << it->second << '\n';                 // 2011
    } else {
        std::cout << "missing " << it->first << '\n';    // 旧写法在 else 里用不了 it
    }

    // 验证 2：init 恰好求值一次
    int calls = 0;
    auto next_id = [&calls] { return ++calls; };
    if (int id = next_id(); id == 1) {
        std::cout << id << ' ' << calls << '\n';         // 1 1
    }
    std::cout << calls << '\n';                          // 1 —— 没有第二次调用

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 与结构化绑定组合（第 11 课的黄金搭档）
    if (auto [pos, ok] = m.insert({"rust", 2015}); ok) {
        std::cout << "inserted " << pos->first << '\n';  // inserted rust
    } else {
        std::cout << "exists " << pos->second << '\n';
    }
    if (auto [pos, ok] = m.insert({"cpp", 9999}); ok) {
        std::cout << "inserted\n";                       // 走不到
    } else {
        std::cout << "exists " << pos->second << '\n';   // exists 2011
    }

    // 2.2 switch 的初始化语句：先归一化，再匹配
    char input = 'Q';
    switch (const char c = static_cast<char>(std::tolower(input)); c) {
        case 'q':
            std::cout << "quit\n";                       // quit —— 'Q' 小写化后命中
            break;
        default:
            std::cout << "unknown\n";
            break;
    }

    // 2.3 表达式也能当 init（分号不能省）
    int x = 5;
    if (x *= 2; x > 9) {
        std::cout << x << '\n';                          // 10
    }

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 查表双分支：找到/找不到两条路都自然
    std::map<std::string, std::string> conf{{"theme", "dark"}};
    if (auto it = conf.find("theme"); it != conf.end()) {
        std::cout << "theme=" << it->second << '\n';     // theme=dark
    }
    if (auto it = conf.find("lang"); it != conf.end()) {
        std::cout << "lang=" << it->second << '\n';      // 走不到
    } else {
        std::cout << "lang missing, use default\n";      // lang missing, use default
    }

    return 0;
}
