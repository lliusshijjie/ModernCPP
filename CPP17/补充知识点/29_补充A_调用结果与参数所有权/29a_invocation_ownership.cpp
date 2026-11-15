// 补充 29A：统一调用之外，还要决定引用、所有权与值类别。
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

struct Meter {
    int value;
    int add(int amount) noexcept { value += amount; return value; }
};

template <class F, class... Args>
decltype(auto) call(F&& function, Args&&... args)
    noexcept(std::is_nothrow_invocable_v<F&&, Args&&...>) {
    return std::invoke(std::forward<F>(function), std::forward<Args>(args)...);
}

constexpr int add(int left, int right) { return left + right; }
static_assert(std::apply(add, std::pair<int, int>{2, 3}) == 5); // apply 在 C++17 已为 constexpr。

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        std::cout << std::boolalpha;
        std::cout << "===== 1. PRINCIPLES =====\n";
        using Field = decltype(&Meter::value);
        static_assert(std::is_same_v<std::invoke_result_t<Field, Meter&>, int&>);
        static_assert(std::is_same_v<std::invoke_result_t<Field, const Meter&>, const int&>);
        static_assert(std::is_same_v<std::invoke_result_t<Field, Meter&&>, int&&>);
        static_assert(std::is_invocable_v<decltype(&Meter::add), Meter&, int>);
        static_assert(!std::is_invocable_v<decltype(&Meter::add), const Meter&, int>);
        Meter meter{10};
        static_assert(noexcept(call(&Meter::add, meter, 1)));
        auto snapshot = call(&Meter::value, meter);       // auto 得到独立 int。
        decltype(auto) alias = call(&Meter::value, meter); // 保留 int&。
        alias += 5;
        require(snapshot == 10 && meter.value == 15, "result category lost");
        std::cout << "snapshot/member: " << snapshot << ' ' << meter.value << '\n'; // 10 15

        std::cout << "===== 2. SYNTAX =====\n";
        auto owned = std::make_tuple(meter, 2);              // 复制 Meter。
        auto borrowed = std::make_tuple(std::ref(meter), 3); // ref 被解包为 Meter&。
        static_assert(std::is_same_v<decltype(borrowed), std::tuple<Meter&, int>>);
        const int copy_result = std::apply(&Meter::add, owned);
        const int reference_result = std::apply(&Meter::add, borrowed);
        require(copy_result == 17 && reference_result == 18 && meter.value == 18,
                "tuple ownership mismatch");
        std::cout << "copy/original: " << std::get<0>(owned).value
                  << ' ' << meter.value << '\n'; // 17 18

        std::cout << "===== 3. APPLICATION =====\n";
        auto arguments = std::make_tuple(std::make_unique<int>(42));
        auto consume = [](std::unique_ptr<int> value) { return *value; };
        static_assert(!std::is_invocable_v<decltype(consume), std::unique_ptr<int>&>);
        static_assert(std::is_invocable_v<decltype(consume), std::unique_ptr<int>&&>);
        const int result = std::apply(consume, std::move(arguments));
        require(result == 42 && std::get<0>(arguments) == nullptr, "resource not transferred");
        std::cout << "consumed: " << result
                  << " source empty=" << (std::get<0>(arguments) == nullptr) << '\n'; // 42 true

        auto task = [owned_value = std::make_unique<int>(7)] { return *owned_value; };
        static_assert(!std::is_copy_constructible_v<decltype(task)>);
        const int task_result = task(); // 闭包本身可以使用，不必做类型擦除。
        require(task_result == 7, "move-only task failed");
        std::cout << "move-only task: " << task_result << '\n'; // 7
#ifdef LESSON29A_COPY_TARGET
        std::function<int()> erased = std::move(task); // 诊断实验：C++17 要求目标可拷贝。
        (void)erased;
#endif
        std::cout << "checks: PASS\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
