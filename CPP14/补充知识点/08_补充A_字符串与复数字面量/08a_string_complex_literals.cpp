// 补充 08A：字符串与复数字面量（C++14）
// ① 原理验证 -> ② 语法演示 -> ③ 应用实战；输出仅使用 ASCII。
// 从 ModernCPP 目录编译：
// g++ -std=c++14 -Wall -Wextra -Wpedantic "CPP14/补充知识点/08_补充A_字符串与复数字面量/08a_string_complex_literals.cpp" -o run

#include <complex>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <type_traits>

int main() {
    using namespace std::string_literals;
    using namespace std::complex_literals;
    std::cout << std::boolalpha;

    std::cout << "===== 1. PRINCIPLES =====\n";
    auto raw = "hello";
    auto text = "hello"s;
    static_assert(std::is_same<decltype(raw), const char*>::value,
                  "plain literal decays in auto deduction");
    static_assert(std::is_same<decltype(text), std::string>::value,
                  "s suffix creates std::string");
    std::cout << text.size() << '\n';                  // 5

    std::cout << "===== 2. SYNTAX =====\n";
    const std::string from_c_string = "a\0b";
    const auto payload = "a\0b"s;
    const std::string explicit_length("a\0b", 3);
    std::cout << from_c_string.size() << ' ' << payload.size() << '\n'; // 1 3
    std::cout << (payload == explicit_length) << ' '
              << (payload[1] == '\0') << '\n';                       // true true

    const auto z = 1.0 + 2.0i;
    static_assert(std::is_same<decltype(2.0i), std::complex<double>>::value,
                  "i suffix uses double");
    static_assert(std::is_same<decltype(2.0if), std::complex<float>>::value,
                  "if suffix uses float");
    static_assert(std::is_same<decltype(2.0il), std::complex<long double>>::value,
                  "il suffix uses long double");
    std::cout << z.real() << ' ' << z.imag() << '\n';                  // 1 2

    std::cout << "===== 3. APPLICATION =====\n";
    const auto message = "user="s + "alice";
    std::cout << message << '\n';                                     // user=alice
    const std::map<std::string, int> records{{"a"s, 1}, {payload, 2}};
    std::cout << records.size() << ' ' << records.at(payload) << '\n'; // 2 2
    std::cout << std::strlen(payload.c_str()) << ' '
              << payload.size() << '\n';                             // 1 3
}
