// 补充 20A：所有权、长度边界与安全的接口适配（C++17）。
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        std::cout << std::boolalpha;
        std::cout << "===== 1. PRINCIPLES =====\n";
        std::string source = "gate|tail";
        const std::string_view head{source.data(), 4};
        const std::string snapshot{head};  // 拷贝字符，取得独立所有权。
        source[0] = 'G';                   // 原地改字符，没有使视图失效。
        std::cout << "view: " << head << '\n';          // Gate
        std::cout << "snapshot: " << snapshot << '\n'; // gate
        require(head == "Gate" && snapshot == "gate", "ownership mismatch");

        std::cout << "===== 2. SYNTAX =====\n";
        // data() 不携带视图长度；这里底层 string 仍活着且末尾有零字符。
        std::cout << "view size: " << head.size() << '\n';             // 4
        std::cout << "C length: " << std::strlen(head.data()) << '\n'; // 9
        const std::string bounded{head};
        std::cout << "owned C length: " << std::strlen(bounded.c_str())
                  << '\n';                                           // 4
        const std::string payload{"a\0b", 3};
        const std::string_view zero_terminated{payload.data()};
        const std::string_view explicit_length{payload.data(), payload.size()};
        std::cout << "lengths: " << zero_terminated.size() << ' '
                  << explicit_length.size() << '\n';                  // 1 3
        require(zero_terminated.size() == 1 && explicit_length.size() == 3,
                "embedded zero lost");

        std::cout << "===== 3. APPLICATION =====\n";
        const std::string line = "  user=alice  ";
        std::string_view field{line};
        field.remove_prefix(2);
        field.remove_suffix(2);
        const auto separator = field.find('=');
        require(separator != std::string_view::npos, "missing separator");
        const auto key = field.substr(0, separator);
        const auto value = field.substr(separator + 1);
        const std::string saved_value{value}; // 需要独立保存时，再取得所有权。
        std::cout << key << ": " << saved_value << '\n'; // user: alice
        std::cout << "source unchanged: " << (line == "  user=alice  ") << '\n';
        require(key == "user" && saved_value == "alice", "parse failed");
        std::cout << "checks: PASS\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
