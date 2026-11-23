// 补充 41A：结构化类值作为模板实参，按编译期名字选字段。

#include <cstddef>
#include <iostream>
#include <string_view>
#include <type_traits>

namespace {

template<std::size_t N>
struct FixedString {
    char value[N]{};

    constexpr FixedString(const char (&text)[N]) {
        for (std::size_t i = 0; i < N; ++i) {
            value[i] = text[i];
        }
    }

    constexpr std::string_view view() const noexcept {
        return {value, N - 1};
    }
};

template<FixedString Name>
struct Field {
    static constexpr auto name = Name.view();
};

struct Event {
    int level;
    int pid;
};

template<FixedString Name>
concept EventField = Name.view() == "level" || Name.view() == "pid";

template<FixedString Name>
    requires EventField<Name>
constexpr int read_field(const Event& event) noexcept {
    if constexpr (Name.view() == "level") {
        return event.level;
    } else {
        return event.pid;
    }
}

} // namespace

int main() {
    std::cout << std::boolalpha;

    // ① 原理验证：字符数组内容相同，得到同一模板特化。
    constexpr char tag[] = "level";
    constexpr bool same_type = std::is_same_v<Field<"level">, Field<FixedString{tag}>>;
    static_assert(same_type && !std::is_same_v<Field<"level">, Field<"pid">>);
    std::cout << "same specialization=" << same_type << '\n'; // same specialization=true

    // ② 语法演示：字面量构造拥有字符的 FixedString，约束排除未知字段。
    static_assert(Field<"level">::name.size() == 5);
    static_assert(EventField<"pid"> && !EventField<"unknown">);
    std::cout << "field=" << Field<"level">::name << '\n'; // field=level

    // ③ 应用实战：字段选择固定，事件值由调用方提供。
    constexpr Event preset{2, 41};
    static_assert(read_field<"level">(preset) == 2 && read_field<"pid">(preset) == 41);

    Event event{3, 42};
    std::cout << "level=" << read_field<"level">(event)
              << " pid=" << read_field<"pid">(event) << '\n'; // level=3 pid=42
}
