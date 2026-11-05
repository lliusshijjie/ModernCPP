// 补充 15A：CTAD 推导与初始化（C++17）。
// ① 原理验证 → ② 语法演示 → ③ 应用实战。
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

template <class T>
struct Label {
    T text;
};

template <class T>
Label(T) -> Label<T>;
Label(const char*) -> Label<std::string>;

int main() {
    std::cout << "===== 1. PRINCIPLES =====\n";
    Label number{42};
    Label literal{"north"};
    static_assert(std::is_same_v<decltype(number), Label<int>>);
    static_assert(std::is_same_v<decltype(literal), Label<std::string>>);
    static_assert(std::is_aggregate_v<decltype(literal)>);
    std::cout << number.text << ' ' << literal.text << '\n'; // 42 north

    std::cout << "===== 2. SYNTAX =====\n";
    std::vector elements{3, 7}; // 两个元素。
    std::vector repeated(3, 7); // 三个 7。
    static_assert(std::is_same_v<decltype(elements), decltype(repeated)>);
    std::cout << elements.size() << ' ' << elements.front() << '\n'; // 2 3
    std::cout << repeated.size() << ' ' << repeated.front() << '\n'; // 3 7

    std::cout << "===== 3. APPLICATION =====\n";
    char buffer[] = "gate";
    const char* text = buffer;
    Label saved{text};       // 选择 const char* 指引，保存 string。
    Label borrowed{buffer};  // 通用指引匹配 char*，保存指针。
    static_assert(std::is_same_v<decltype(borrowed), Label<char*>>);
    buffer[0] = 'G';
    std::cout << saved.text << ' ' << borrowed.text << '\n'; // gate Gate
}
