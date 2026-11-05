// 补充 16A：optional 惰性兜底与值语义（C++17）。
// ① 原理验证 → ② 语法演示 → ③ 应用实战。
#include <iostream>
#include <optional>
#include <string>

std::string display_title(const std::optional<std::string>& custom_title,
                          int& fallback_calls) {
    if (custom_title) {
        return *custom_title; // 返回独立的值。
    }
    ++fallback_calls;
    return "Untitled exhibition";
}

int main() {
    std::cout << "===== 1. PRINCIPLES =====\n";
    int calls = 0;
    const auto build_default = [&calls] {
        ++calls;
        return std::string{"default"};
    };
    const std::optional<std::string> title{"Ancient maps"};
    const auto eager = title.value_or(build_default());
    std::cout << eager << " calls=" << calls << '\n'; // Ancient maps calls=1

    std::cout << "===== 2. SYNTAX =====\n";
    calls = 0;
    const auto lazy = title ? *title : build_default();
    std::cout << lazy << " calls=" << calls << '\n'; // Ancient maps calls=0
    const std::optional<std::string> missing;
    const auto fallback = missing ? *missing : build_default();
    std::cout << fallback << " calls=" << calls << '\n'; // default calls=1

    std::cout << "===== 3. APPLICATION =====\n";
    std::optional<std::string> custom{"Night tour"};
    const std::string snapshot = display_title(custom, calls);
    custom.reset();
    std::cout << snapshot << '\n'; // Night tour
    const std::string default_title = display_title(custom, calls);
    std::cout << default_title << " calls=" << calls << '\n'; // Untitled exhibition calls=2
    if (eager != "Ancient maps" || lazy != "Ancient maps" || calls != 2) return 1;
}
