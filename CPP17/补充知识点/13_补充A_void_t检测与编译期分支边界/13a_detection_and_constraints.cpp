// 补充 13A：void_t 检测与编译期分支边界（C++17）。
// ① 原理验证 -> ② 语法演示 -> ③ 应用实战；输出仅使用 ASCII。
// 从 ModernCPP 目录编译：
// g++ -std=c++17 -Wall -Wextra -Wpedantic "CPP17/补充知识点/13_补充A_void_t检测与编译期分支边界/13a_detection_and_constraints.cpp" -o run

#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

template <class T> using plain_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <class T, class = void> struct has_size : std::false_type {};
template <class T>
struct has_size<T, std::void_t<decltype(std::declval<const T&>().size())>>
    : std::bool_constant<std::is_convertible_v<decltype(std::declval<const T&>().size()),
                                              std::size_t>> {};

struct MutableOnly { std::size_t size() { return 1; } };
struct WrongResult { std::string size() const { return "one"; } };

template <class T>
std::size_t size_or_zero(const T& value) {
    if constexpr (has_size<T>::value) return static_cast<std::size_t>(value.size());
    else return 0; // 对 int 实例，value.size() 分支不实例化。
}

// SFINAE 控制候选集合；if constexpr 只控制被选中函数体中的实现。
template <class T>
std::enable_if_t<has_size<T>::value, std::size_t> required_size(const T& value) {
    return static_cast<std::size_t>(value.size());
}

template <class T, class = void> struct has_value_type : std::false_type {};
template <class T>
struct has_value_type<T, std::void_t<typename T::value_type>> : std::true_type {};
template <class T>
struct integral_element : std::is_integral<typename T::value_type> {};
template <class T>
using integral_container = std::conjunction<has_value_type<T>, integral_element<T>>;

template <class T>
const char* classify(const T&) {
    if constexpr (has_value_type<T>::value) {
        // 分两层：只有确认成员类型存在后，才形成这个条件表达式。
        if constexpr (std::is_integral_v<typename T::value_type>) return "integral-elements";
        else return "other-elements";
    } else {
        return "no-value-type";
    }
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";
    static_assert(!std::is_integral_v<const int&>);
    static_assert(std::is_integral_v<plain_t<const int&>>);
    std::cout << std::is_integral_v<const int&> << ' '
              << std::is_integral_v<plain_t<const int&>> << '\n'; // false true
    static_assert(has_size<std::string>::value);
    static_assert(!has_size<int>::value && !has_size<MutableOnly>::value);
    static_assert(!has_size<WrongResult>::value);
    std::cout << has_size<std::string>::value << ' ' << has_size<MutableOnly>::value
              << ' ' << has_size<WrongResult>::value << '\n'; // true false false

    std::cout << "===== 2. SYNTAX =====\n";
    static_assert(!integral_container<int>::value); // 不实例化 integral_element<int> 的定义。
    static_assert(integral_container<std::vector<int>>::value);
    std::cout << integral_container<int>::value << ' '
              << integral_container<std::vector<int>>::value << '\n'; // false true
    // 不能用 has_value_type<T>::value && is_integral_v<typename T::value_type>
    // 来隐藏 int::value_type；条件表达式本身也必须合法。

    std::cout << "===== 3. APPLICATION =====\n";
    const std::string text = "cpp";
    std::cout << size_or_zero(text) << ' ' << size_or_zero(42) << '\n'; // 3 0
    std::cout << required_size(text) << '\n'; // 3
    std::cout << classify(std::vector<int>{1}) << ' ' << classify(42) << '\n';
    // integral-elements no-value-type
    if (size_or_zero(text) != 3 || size_or_zero(WrongResult{}) != 0) return 1;
    // required_size(42); // 取消注释：候选因 SFINAE 退出，没有匹配重载。
}
