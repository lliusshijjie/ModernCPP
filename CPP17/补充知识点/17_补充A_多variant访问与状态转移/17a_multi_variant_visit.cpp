// 补充 17A：多 variant 访问与状态转移（C++17）。
// ① 原理验证 → ② 语法演示 → ③ 应用实战。
#include <iostream>
#include <optional>
#include <variant>

template <class... T>
struct overloaded : T... {
    using T::operator()...;
};
template <class... T>
overloaded(T...) -> overloaded<T...>;

struct Idle {};
struct Running { int chunks; };
struct Finished { int chunks; };
using State = std::variant<Idle, Running, Finished>;

struct Start {};
struct Chunk {};
struct Finish {};
using Event = std::variant<Start, Chunk, Finish>;

std::optional<State> next_state(const State& state, const Event& event) {
    return std::visit(overloaded{
        [](const Idle&, const Start&) -> std::optional<State> {
            return State{Running{0}};
        },
        [](const Running& running, const Chunk&) -> std::optional<State> {
            return State{Running{running.chunks + 1}};
        },
        [](const Running& running, const Finish&) -> std::optional<State> {
            return State{Finished{running.chunks}};
        },
        [](const auto&, const auto&) -> std::optional<State> {
            return std::nullopt; // 明确拒绝其余组合。
        }
    }, state, event);
}

int main() {
    std::cout << std::boolalpha << "===== 1. PRINCIPLES =====\n";
    State state = Idle{};
    std::cout << next_state(state, Chunk{}).has_value() << '\n'; // false
    std::cout << next_state(state, Start{}).has_value() << '\n'; // true

    std::cout << "===== 2. SYNTAX =====\n";
    if (auto next = next_state(state, Start{}); next) {
        state = *next;
    }
    std::cout << std::holds_alternative<Running>(state) << '\n'; // true

    std::cout << "===== 3. APPLICATION =====\n";
    const Event events[]{Chunk{}, Chunk{}, Finish{}, Chunk{}};
    for (const auto& event : events) {
        if (auto next = next_state(state, event); next) {
            state = *next; // 先算出独立的结果，再替换当前状态。
        } else {
            std::cout << "rejected\n"; // 已完成的任务不能继续接收分块。
        }
    }
    if (const auto* finished = std::get_if<Finished>(&state);
        finished && finished->chunks == 2) {
        std::cout << "finished chunks=" << finished->chunks << '\n'; // finished chunks=2
        return 0;
    }
    return 1;
}
