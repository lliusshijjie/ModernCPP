// 补充 19A：缺失值与错误的分层建模（C++17）。
// ① 原理验证 → ② 语法演示 → ③ 应用实战。
#include <any>
#include <iostream>
#include <optional>
#include <utility>
#include <variant>

struct PulseCount { int ticks; };
struct Duration { int milliseconds; };
enum class DecodeError { empty_payload, wrong_type, out_of_range };

using Frame = std::optional<std::any>;
using Outcome = std::variant<Duration, DecodeError>;
using Decoded = std::optional<Outcome>;

Decoded decode(const Frame& frame) {
    if (!frame) return std::nullopt;
    if (!frame->has_value()) return Outcome{DecodeError::empty_payload};
    const auto* count = std::any_cast<PulseCount>(&*frame);
    if (!count) return Outcome{DecodeError::wrong_type};
    if (count->ticks < 0 || count->ticks > 1'000) return Outcome{DecodeError::out_of_range};
    return Outcome{Duration{count->ticks * 10}};
}

void show(const Decoded& decoded) {
    if (!decoded) {
        std::cout << "no frame\n";
        return;
    }
    if (const auto* duration = std::get_if<Duration>(&*decoded); duration) {
        std::cout << duration->milliseconds << " ms\n";
        return;
    }
    switch (std::get<DecodeError>(*decoded)) {
    case DecodeError::empty_payload: std::cout << "empty payload\n"; break;
    case DecodeError::wrong_type: std::cout << "wrong type\n"; break;
    case DecodeError::out_of_range: std::cout << "out of range\n"; break;
    }
}

int main() {
    std::cout << "===== 1. PRINCIPLES =====\n";
    show(decode(std::nullopt));              // no frame
    show(decode(Frame{std::in_place}));      // empty payload：外层有值，内层 any 为空。

    std::cout << "===== 2. SYNTAX =====\n";
    show(decode(Frame{std::any{0}}));        // wrong type：int 与 PulseCount 类型不同。
    show(decode(Frame{std::any{PulseCount{0}}})); // 0 ms：零是合法数据。

    std::cout << "===== 3. APPLICATION =====\n";
    const Frame batch[]{
        Frame{std::any{PulseCount{25}}},
        Frame{std::any{PulseCount{-1}}},
        Frame{std::any{PulseCount{1'001}}},
        Frame{std::any{PulseCount{1'000}}}
    };
    for (const auto& frame : batch) {
        show(decode(frame)); // 250 ms / out of range / out of range / 10000 ms
    }
}
