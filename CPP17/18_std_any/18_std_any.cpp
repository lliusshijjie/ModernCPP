// 第 18 课示例：std::any（C++17）
//
// ① 原理验证：按值持有、精确类型匹配
// ② 语法演示：取值、原位构造、清空与异常后的状态
// ③ 应用实战：读取扩展组件的配置
//
// g++ -std=c++17 -Wall -Wextra -Wpedantic -pedantic-errors 18_std_any.cpp -o run
// 先预测输出，再运行对照。

#include <any>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

struct ThrowOnCopy {
    ThrowOnCopy() = default;
    ThrowOnCopy(const ThrowOnCopy&) {
        throw std::runtime_error("copy failed");
    }
};

using ExtensionOptions = std::map<std::string, std::any>;

void printIntOption(const ExtensionOptions& options, const std::string& key) {
    std::cout << key << ": ";
    if (const auto it = options.find(key); it != options.end()) {
        if (const auto* value = std::any_cast<int>(&it->second)) {
            std::cout << *value << '\n';
        } else {
            std::cout << "expected int\n";
        }
    } else {
        std::cout << "missing\n";
    }
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    std::any value;
    std::cout << "empty: " << !value.has_value() << '\n';  // empty: true

    const int original = 7;
    value = original;  // 存入一个独立的 int
    std::any copy = value;
    std::any_cast<int&>(value) = 9;
    std::cout << "original / stored / copy: "
              << original << " / " << std::any_cast<int>(value) << " / "
              << std::any_cast<int>(copy) << '\n';  // original / stored / copy: 7 / 9 / 7
    std::cout << "stored type is int: "
              << (value.type() == typeid(int)) << '\n';  // stored type is int: true
    std::cout << "read as double failed: "
              << (std::any_cast<double>(&value) == nullptr) << '\n';  // read as double failed: true

    std::cout << "===== 2. SYNTAX =====\n";

    try {
        static_cast<void>(std::any_cast<double>(value));
    } catch (const std::bad_any_cast&) {
        std::cout << "caught bad_any_cast\n";  // caught bad_any_cast
    }

    value = "ready";
    std::cout << "literal stored as const char*: "
              << (std::any_cast<const char*>(&value) != nullptr) << '\n';  // literal stored as const char*: true

    std::any text{std::in_place_type<std::string>, 3, 'a'};
    auto numbers = std::make_any<std::vector<int>>({1, 2, 3});
    text.swap(numbers);
    const auto& items = std::any_cast<const std::vector<int>&>(text);
    std::cout << "after swap: " << items.size() << " items, "
              << std::any_cast<const std::string&>(numbers) << '\n';  // after swap: 3 items, aaa

    auto& newText = value.emplace<std::string>("ready");
    newText += "!";
    std::cout << "after emplace: " << std::any_cast<const std::string&>(value)
              << '\n';  // after emplace: ready!
    value.reset();
    std::cout << "reset type is void: "
              << (value.type() == typeid(void)) << '\n';  // reset type is void: true

    const auto source = std::make_any<ThrowOnCopy>();
    std::any target = 42;
    try {
        target = source;
    } catch (const std::runtime_error&) {
        std::cout << "copy assignment failed, retained: "
                  << std::any_cast<int>(target) << '\n';  // copy assignment failed, retained: 42
    }
    try {
        target.emplace<ThrowOnCopy>(std::any_cast<const ThrowOnCopy&>(source));
    } catch (const std::runtime_error&) {
        std::cout << "emplace failed, empty: " << !target.has_value()
                  << '\n';  // emplace failed, empty: true
    }

    std::cout << "===== 3. APPLICATION =====\n";

    const ExtensionOptions options{
        {"retry_count", 3},
        {"timeout_ms", 250.0},
        {"output_dir", std::string("reports")},
    };
    printIntOption(options, "retry_count");  // retry_count: 3
    printIntOption(options, "timeout_ms");   // timeout_ms: expected int
    printIntOption(options, "buffer_size");  // buffer_size: missing
}
