// 第 20 课示例：std::string_view（C++17）
//
// 结构：① 原理验证 —— 两个词的视图；零拷贝借数据；O(1) 切片
//       ② 语法演示 —— 三种来源；内容比较；at 越界；显式转 string
//       ③ 应用实战 —— key=value 解析器（零分配）；统一只读接口
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 20_string_view.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

// ③ 去首尾空格：纯视图操作，零分配
std::string_view trim(std::string_view sv) {
    while (!sv.empty() && sv.front() == ' ') sv.remove_prefix(1);
    while (!sv.empty() && sv.back() == ' ') sv.remove_suffix(1);
    return sv;                                 // 视图指向调用方的数据 —— 安全
}

// ③ key=value 解析：全程只有视图在飞
bool parse_kv(std::string_view line,
              std::string_view& key, std::string_view& val) {
    auto eq = line.find('=');
    if (eq == std::string_view::npos) return false;
    key = trim(line.substr(0, eq));
    val = trim(line.substr(eq + 1));
    return true;
}

// ③ 统一只读接口：一个形参吃下三种调用方
void greet(std::string_view name) {
    std::cout << "hello " << name << '\n';
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：视图就是两个词（x64：指针 8 + 长度 8）
    std::cout << sizeof(std::string_view) << '\n';               // 常见 64 位实现为 16，非标准固定值

    // 验证 2：借 string 的缓冲 —— data() 指向同一块内存，零拷贝
    std::string s = "modern-cpp";
    std::string_view sv = s;
    std::cout << (sv.data() == s.data()) << ' '
              << sv.size() << '\n';                              // true 10

    // 验证 3：view::substr 是 O(1)；string::substr 是 O(n)，复制字符，可能分配。
    auto head_v = sv.substr(0, 6);                               // 视图切片
    std::string head_s = s.substr(0, 6);                         // 拷贝切片
    std::cout << head_v << ' ' << head_s << ' '
              << (head_v.data() == s.data()) << ' '              // true：借的
              << (head_s.data() != s.data()) << '\n';            // true：独立存储，不证明发生堆分配

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 三种来源：字面量 / string / 指针+长度
    std::string_view a = "literal";
    std::string_view b = s;
    std::string_view c{s.data() + 7, 3};
    std::cout << a << ' ' << b << ' ' << c << '\n';              // literal modern-cpp cpp

    // 2.2 比较按内容（不是指针）：不同存储的同内容相等
    std::string other = "literal";
    std::string_view d = other;                                  // 另一块内存
    std::cout << (a == d) << ' ' << (a.data() != d.data()) << '\n';  // true true

    // 2.3 operator[] 快而险（越界 UB）；at() 慢而稳（越界抛）
    try {
        (void)a.at(100);
        std::cout << "no throw\n";                               // 走不到
    } catch (const std::out_of_range&) {
        std::cout << "out_of_range\n";                           // out_of_range
    }

    // 2.4 view → string 是 explicit：防止隐性拷贝税
    std::string copied(a);                                       // 必须显式
    // std::string bad = a;                                      // error: explicit
    std::cout << copied << '\n';                                 // literal

    // 2.5 remove_prefix/suffix 原地缩窗口
    std::string_view t = "  pad  ";
    std::cout << '[' << trim(t) << ']' << '\n';                  // [pad]

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 key=value 解析器：零分配，视图切分 + trim
    std::string_view lines[] = {"  name = modern cpp  ", "lang = cpp", "broken-line"};
    for (auto line : lines) {
        std::string_view key, val;
        if (parse_kv(line, key, val)) {
            std::cout << '[' << key << "]=[" << val << "]\n";
        } else {
            std::cout << "skip: " << line << '\n';
        }
    }
    // [name]=[modern cpp]
    // [lang]=[cpp]
    // skip: broken-line

    // 3.2 统一只读接口：string / 字面量 / 字符数组，一个形参全接住
    std::string user = "Tom";
    greet(user);                    // hello Tom
    greet("literal-Jerry");         // hello literal-Jerry —— 无临时 string
    char buf[] = "array-Kate";
    greet(buf);                     // hello array-Kate

    return 0;
}
