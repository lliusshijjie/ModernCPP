// 补充 22A：属性提供诊断提示，不会删除表达式的副作用。
#include <iostream>
#include <stdexcept>

struct [[nodiscard]] Validation {
    bool ok;
};

Validation validate(int input, int& calls) {
    ++calls;
    return {input >= 0};
}

Validation& last_result() {
    static Validation result{true};
    return result; // 返回引用，不自动继承类属性的丢弃诊断。
}

void debug_trace([[maybe_unused]] int value) {
#ifndef NDEBUG
    std::cout << "debug value: " << value << '\n';
#endif
}

enum class Detail { Basic, Full };

void show_help(Detail detail) {
    switch (detail) {
    case Detail::Full:
        std::cout << "advanced options\n";
        [[fallthrough]]; // 下一个标签仍属于同一个 switch。
    case Detail::Basic:
        std::cout << "basic options\n";
        break;
    }
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";
    int calls = 0;
    const auto result = validate(3, calls);
    std::cout << "valid: " << result.ok << '\n'; // true
    (void)validate(-1, calls); // 明确选择忽略；调用仍然执行。
    last_result();            // 类型属性不覆盖引用返回。
#ifdef LESSON22A_DISCARD
    validate(7, calls);       // 仅用于编译器诊断实验，默认不编译这一行。
#endif
    std::cout << "calls: " << calls << '\n';    // 2

    std::cout << "===== 2. SYNTAX =====\n";
    int evaluated = 0;
    debug_trace(++evaluated); // Release 仍求值，maybe_unused 不改变执行语义。
    std::cout << "argument evaluations: " << evaluated << '\n'; // 1

    std::cout << "===== 3. APPLICATION =====\n";
    show_help(Detail::Full);
    if (!result.ok || calls != 2 || evaluated != 1) {
        throw std::runtime_error("attribute semantics changed");
    }
    std::cout << "checks: PASS\n";
}
