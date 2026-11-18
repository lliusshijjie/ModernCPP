#pragma once
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace tracelab {
// [40,44] Scratch strings live only during constant evaluation. The table owns its bytes.
template <std::size_t N>
consteval auto schema(std::array<std::string_view, N> fields) {
    std::array<std::array<char, 16>, N> result{};
    std::vector<std::string> seen;
    for (auto field : fields) {
        std::string key(field);
        if (key.empty() || key.size() >= result[0].size()) {
            throw "field length";
        }
        for (char& c : key) {
            if (c >= 'A' && c <= 'Z') {
                c += 'a' - 'A';
            }
        }
        for (const auto& previous : seen) {
            if (key == previous) {
                throw "duplicate field";
            }
        }
        key.copy(result[seen.size()].data(), key.size());
        seen.push_back(key);
    }
    return result;
}

inline constexpr auto fields = schema(std::array<std::string_view, 7>{
    "timestamp", "level", "component", "pid", "flags", "duration_us", "message"});
} // namespace tracelab
