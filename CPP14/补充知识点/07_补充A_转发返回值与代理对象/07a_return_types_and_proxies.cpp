// 补充 07A：转发返回值与代理对象（C++14）
// ① 原理验证 -> ② 语法演示 -> ③ 应用实战；输出仅使用 ASCII。
// 从 ModernCPP 目录编译：
// g++ -std=c++14 -Wall -Wextra -Wpedantic "CPP14/补充知识点/07_补充A_转发返回值与代理对象/07a_return_types_and_proxies.cpp" -o run

#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

template <class Container>
decltype(auto) element(Container& c, std::size_t index) {
    return c[index];
}

decltype(auto) make_value() {
    int local = 7;
    return local; // 未加括号的名字：按声明类型推导，返回 int。
}

template <class T>
auto size_or_zero(const T& value) -> decltype(value.size()) {
    return value.size();
}

std::size_t size_or_zero(...) { return 0; }

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";
    std::vector<int> numbers{10, 20};
    const std::vector<int> readonly{30, 40};
    static_assert(std::is_same<decltype(element(numbers, 0)), int&>::value,
                  "mutable vector returns int&");
    static_assert(std::is_same<decltype(element(readonly, 0)), const int&>::value,
                  "const vector returns const int&");
    element(numbers, 0) = 99;
    std::cout << numbers[0] << ' ' << element(readonly, 0) << '\n'; // 99 30

    std::cout << "===== 2. SYNTAX =====\n";
    std::vector<bool> bits{false, true};
    decltype(auto) proxy = element(bits, 0);
    static_assert(std::is_same<decltype(proxy), std::vector<bool>::reference>::value,
                  "prvalue proxy is returned by value, not as proxy&&");
    proxy = true;
    bool snapshot = bits[0];
    bits[0] = false;
    std::cout << static_cast<bool>(proxy) << ' ' << snapshot << '\n'; // false true

    std::string text = "alive";
    decltype(auto) alias = std::move(text);
    static_assert(std::is_same<decltype(alias), std::string&&>::value,
                  "move expression binds a reference here");
    alias += '!'; // 修改同一个字符串，没有构造第二个字符串对象。
    std::cout << text << '\n'; // alive!

    std::cout << "===== 3. APPLICATION =====\n";
    static_assert(std::is_same<decltype(make_value()), int>::value,
                  "unparenthesized local name returns a value");
    std::cout << make_value() << '\n';                 // 7
    std::cout << size_or_zero(text) << ' '
              << size_or_zero(42) << '\n';            // 6 0
    // element(std::vector<int>{1, 2}, 0); // 临时容器不能绑定到此接口。
    // 不要从函数中 return (local) 或以引用形式返回 std::move(local)。
}
