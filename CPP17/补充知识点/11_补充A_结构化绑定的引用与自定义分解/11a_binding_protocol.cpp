// 补充 11A：结构化绑定的引用与自定义分解（C++17）。
// ① 原理验证 -> ② 语法演示 -> ③ 应用实战；输出仅使用 ASCII。
// 从 ModernCPP 目录编译：
// g++ -std=c++17 -Wall -Wextra -Wpedantic "CPP17/补充知识点/11_补充A_结构化绑定的引用与自定义分解/11a_binding_protocol.cpp" -o run

#include <cstddef>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sensor {
struct Reading { int channel; double value; bool valid; };

// get 放在类型所属命名空间，由 ADL 找到；不向 std 添加 get 重载。
template <std::size_t I, class T>
decltype(auto) field(T&& reading) {
    static_assert(I < 2, "Reading exposes two tuple elements");
    if constexpr (I == 0) return (std::forward<T>(reading).channel);
    else return (std::forward<T>(reading).value);
}
template <std::size_t I> decltype(auto) get(Reading& r) { return field<I>(r); }
template <std::size_t I> decltype(auto) get(const Reading& r) { return field<I>(r); }
template <std::size_t I> decltype(auto) get(Reading&& r) { return field<I>(std::move(r)); }
template <std::size_t I> decltype(auto) get(const Reading&& r) { return field<I>(std::move(r)); }
} // namespace sensor

// 标准允许为此用户自定义类型提供符合协议的类模板特化。
namespace std {
template <> struct tuple_size<sensor::Reading> : integral_constant<size_t, 2> {};
template <size_t I>
struct tuple_element<I, sensor::Reading> : tuple_element<I, tuple<int, double>> {};
} // namespace std

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";
    int source = 7;
    std::tuple<int, int&> row{1, source};
    auto [number, alias] = row; // 隐藏 tuple 被复制，但其引用元素仍指向 source。
    static_assert(std::is_same_v<decltype(number), int>);
    static_assert(std::is_same_v<decltype(alias), int&>);
    static_assert(std::is_same_v<decltype((number)), int&>);
    static_assert(sizeof(number) == sizeof(int)); // 绑定名可以用于 sizeof。
    number = 99;
    alias = 20;
    std::cout << number << ' ' << std::get<0>(row) << ' ' << source << '\n'; // 99 1 20
    auto& [original, reference] = row;
    original = 5;
    std::cout << std::get<0>(row) << ' ' << (&reference == &source) << '\n'; // 5 true

    std::cout << "===== 2. SYNTAX =====\n";
    const auto& [readonly, still_reference] = row;
    static_assert(std::is_same_v<decltype(readonly), const int>);
    static_assert(std::is_same_v<decltype(still_reference), int&>);
    still_reference = 30; // const tuple 不会把 int& 引用的对象变成 const。
    std::cout << readonly << ' ' << source << '\n'; // 5 30
    auto saved = [snapshot = alias] { return snapshot; }; // C++17 使用初始化捕获。
    source = 40;
    std::cout << saved() << ' ' << alias << '\n'; // 30 40

    std::cout << "===== 3. APPLICATION =====\n";
    sensor::Reading reading{2, 18.5, true};
    auto& [channel, value] = reading; // tuple-like 协议只暴露两个字段。
    value = 21.5;
    std::cout << channel << ' ' << reading.value << ' ' << reading.valid << '\n'; // 2 21.5 true
    auto [copied_channel, copied_value] = reading;
    copied_value = 99.0;
    std::cout << copied_channel << ' ' << copied_value << ' ' << reading.value << '\n'; // 2 99 21.5
    const auto& [temporary_channel, temporary_value] = sensor::Reading{3, 2.5, true};
    std::cout << temporary_channel << ' ' << temporary_value << '\n'; // 3 2.5
    static_assert(std::is_same_v<decltype(sensor::get<0>(std::declval<sensor::Reading&&>())), int&&>);
    if (source != 40 || std::get<0>(row) != 5 || reading.value != 21.5) return 1;
}
