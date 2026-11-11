// 补充 24A：先完成词法和业务校验，成功后再修改调用者的结果。
#include <array>
#include <charconv>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <system_error>

enum class ParseError { None, Empty, Invalid, Overflow, Trailing, Domain };

struct [[nodiscard]] ParseReport {
    ParseError error;
    std::size_t consumed;
};

ParseReport parse_port(std::string_view text, unsigned& output) {
    if (text.empty()) return {ParseError::Empty, 0}; // 先处理默认空视图。
    long long candidate = 0;
    const char* last = text.data() + text.size();
    const auto [ptr, error] = std::from_chars(text.data(), last, candidate);
    const auto consumed = static_cast<std::size_t>(ptr - text.data());
    if (error == std::errc::invalid_argument) return {ParseError::Invalid, consumed};
    if (error == std::errc::result_out_of_range) return {ParseError::Overflow, consumed};
    if (ptr != last) return {ParseError::Trailing, consumed};
    if (candidate < 1 || candidate > 65535) return {ParseError::Domain, consumed};
    output = static_cast<unsigned>(candidate); // 唯一提交点。
    return {ParseError::None, consumed};
}

const char* name(ParseError error) {
    switch (error) {
    case ParseError::None: return "ok";
    case ParseError::Empty: return "empty";
    case ParseError::Invalid: return "invalid";
    case ParseError::Overflow: return "overflow";
    case ParseError::Trailing: return "trailing";
    case ParseError::Domain: return "domain";
    }
    return "unknown";
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        std::cout << std::boolalpha;
        std::cout << "===== 1. PRINCIPLES =====\n";
        const std::string_view prefix = "8080ms";
        int candidate = 0;
        const auto [ptr, error] = std::from_chars(
            prefix.data(), prefix.data() + prefix.size(), candidate);
        std::cout << "prefix: " << candidate << ' ' << (error == std::errc{})
                  << ' ' << (ptr == prefix.data() + prefix.size()) << '\n'; // 8080 true false

        std::cout << "===== 2. SYNTAX =====\n";
        struct Case {
            std::string_view text;
            ParseError expected;
            unsigned port;
        };
        const Case cases[] = {
            {std::string_view{}, ParseError::Empty, 443}, {"8080", ParseError::None, 8080},
            {"8080ms", ParseError::Trailing, 443}, {"+80", ParseError::Invalid, 443},
            {" 80", ParseError::Invalid, 443}, {"-1", ParseError::Domain, 443},
            {"0", ParseError::Domain, 443}, {"65536", ParseError::Domain, 443},
            {"999999999999999999999999", ParseError::Overflow, 443},
            {"1", ParseError::None, 1},
            {"65535", ParseError::None, 65535}
        };
        for (const auto& item : cases) {
            unsigned port = 443;
            const auto report = parse_port(item.text, port);
            require(report.error == item.expected && port == item.port, "parse contract failed");
            std::cout << '[' << item.text << "] " << name(report.error)
                      << " consumed=" << report.consumed << " port=" << port << '\n';
        }

        std::cout << "===== 3. APPLICATION =====\n";
        std::array<char, 2> small{};
        const auto failed = std::to_chars(small.data(), small.data() + small.size(), 8080);
        require(failed.ec == std::errc::value_too_large, "small buffer should fail");
        // 失败后的缓冲内容未指定，不能拿来当作截断后的合法数字。
        std::cout << "small buffer rejected: " << (failed.ec != std::errc{}) << '\n'; // true
        std::array<char, 16> buffer{};
        const auto written = std::to_chars(buffer.data(), buffer.data() + buffer.size(), 8080);
        require(written.ec == std::errc{}, "format failed");
        const auto length = static_cast<std::size_t>(written.ptr - buffer.data());
        const std::string_view encoded{buffer.data(), length}; // 按长度读，不要求零结尾。
        require(encoded == "8080", "format mismatch");
        std::cout << "encoded: " << encoded << " length=" << length << '\n'; // 8080 length=4
        std::cout << "checks: PASS\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
