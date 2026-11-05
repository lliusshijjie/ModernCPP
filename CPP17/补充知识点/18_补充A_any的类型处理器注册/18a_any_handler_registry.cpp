// 补充 18A：any 的类型处理器注册（C++17）。
// ① 原理验证 → ② 语法演示 → ③ 应用实战。
#include <any>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <typeindex>

struct Caption {
    std::string text;
    std::string describe() const { return "caption: " + text; }
};
struct Reading {
    int number;
    std::string describe() const { return "reading: " + std::to_string(number); }
};

class Registry {
public:
    template <class T>
    void add() {
        static_assert(std::is_same_v<T, std::decay_t<T>> && std::is_copy_constructible_v<T>);
        // 无捕获 lambda 转成函数指针。键和取值类型都由同一个 T 确定。
        handlers_.emplace(std::type_index(typeid(T)), [](const std::any& value) {
            return std::any_cast<const T&>(value).describe();
        });
    }

    std::optional<std::string> render(const std::any& value) const {
        if (auto found = handlers_.find(std::type_index(value.type()));
            found != handlers_.end()) {
            return found->second(value);
        }
        return std::nullopt;
    }

private:
    using Handler = std::string (*)(const std::any&);
    std::map<std::type_index, Handler> handlers_;
};

int main() {
    std::cout << std::boolalpha << "===== 1. PRINCIPLES =====\n";
    const std::any caption = Caption{"opening"};
    std::cout << (caption.type() == typeid(Caption)) << '\n'; // true
    std::cout << (std::any_cast<Reading>(&caption) == nullptr) << '\n'; // true

    std::cout << "===== 2. SYNTAX =====\n";
    Registry registry;
    registry.add<Caption>();
    std::cout << registry.render(caption).value_or("unregistered") << '\n'; // caption: opening
    const std::any reading = Reading{7};
    std::cout << registry.render(reading).value_or("unregistered") << '\n'; // unregistered

    std::cout << "===== 3. APPLICATION =====\n";
    registry.add<Reading>();
    std::cout << registry.render(reading).value_or("unregistered") << '\n'; // reading: 7
    std::cout << registry.render(std::any{}).value_or("unregistered") << '\n'; // unregistered
    if (registry.render(reading) != "reading: 7" || registry.render(42)) return 1;
}
