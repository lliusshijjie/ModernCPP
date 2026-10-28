// 补充 03A：类型萃取与 SFINAE 约束（C++14）
// ① 原理验证 -> ② 语法演示 -> ③ 应用实战；输出仅使用 ASCII。
// 从 ModernCPP 目录编译：
// g++ -std=c++14 -Wall -Wextra -Wpedantic "CPP14/补充知识点/03_补充A_类型萃取与SFINAE约束/03a_type_traits_sfinae.cpp" -o run

#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

template <class T>
using normalized_t = std::decay_t<T>;

template <class T>
std::enable_if_t<std::is_integral<T>::value, const char*>
number_kind(T) {
    return "integral";
}

template <class T>
std::enable_if_t<std::is_floating_point<T>::value, const char*>
number_kind(T) {
    return "floating";
}

// 标准 std::void_t 属于 C++17；这里用本地辅助类型完成 C++14 检测。
template <class...>
struct MakeVoid { using type = void; };

template <class... Ts>
using void_t14 = typename MakeVoid<Ts...>::type;

template <class T, class = void>
struct has_size : std::false_type {};

template <class T>
struct has_size<T, void_t14<decltype(std::declval<const T&>().size())>>
    : std::true_type {};

template <class T>
std::enable_if_t<has_size<T>::value, std::size_t>
length_of(const T& value) {
    return static_cast<std::size_t>(value.size());
}

struct FinalValue final {};

static_assert(std::is_same<normalized_t<const int&>, int>::value, "decay cv-ref");
static_assert(std::is_same<normalized_t<int[3]>, int*>::value, "array decay");
static_assert(std::is_same<std::remove_reference_t<const int&>, const int>::value,
              "remove_reference preserves const");
static_assert(has_size<std::string>::value, "string has size");
static_assert(!has_size<int>::value, "int has no size");
static_assert(std::is_final<FinalValue>::value, "C++14 is_final");
static_assert(std::integral_constant<int, 3>{}() == 3, "C++14 call operator");

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";
    std::cout << number_kind(7) << '\n';            // integral
    std::cout << number_kind(2.5) << '\n';          // floating
    std::cout << number_kind(true) << '\n';         // integral
    // number_kind(std::string("7")); // 没有可用重载，编译错误。

    std::cout << "===== 2. SYNTAX =====\n";
    using Choice = std::conditional_t<std::is_integral<int>::value, int, char>;
    std::cout << std::is_same<Choice, int>::value << '\n'; // true
    std::cout << has_size<std::vector<int>>{}() << ' '
              << has_size<int>{}() << '\n';               // true false
    std::cout << std::integral_constant<int, 3>{}() << '\n'; // 3

    std::cout << "===== 3. APPLICATION =====\n";
    const std::string text = "hello";
    const std::vector<int> values{10, 20, 30};
    std::cout << length_of(text) << ' ' << length_of(values) << '\n'; // 5 3
    // length_of(42); // has_size<int> 为 false，候选被排除。
}
