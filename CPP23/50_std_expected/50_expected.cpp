// 第 50 课示例：std::expected：错误处理新范式（C++23）
//
// 结构：① 原理验证 —— 值/错误二选一；拆包通道对比
//       ② 语法演示 —— 单子式组合：错误自动短路
//       ③ 应用实战 —— 解析校验流水线（替代错误码+out 参）
//
// 本机（MSYS2 g++ 15.1.0）编译运行——注意链接 -lstdc++exp：
//   g++ -std=c++23 -Wall -Wextra 50_expected.cpp -o run -lstdc++exp && ./run
// 先预测每行输出，再运行对照！

#include <charconv>
#include <expected>
#include <print>
#include <string_view>
#include <system_error>

enum class ParseErr { empty, bad, negative, too_small, too_big };

// ① 完整解析端口 1..65535：允许前导零，不允许符号/空白/尾随文本
[[nodiscard]] std::expected<int, ParseErr> parse_port(std::string_view s) {
    if (s.empty()) return std::unexpected(ParseErr::empty);
    if (s.front() == '-') return std::unexpected(ParseErr::negative);
    if (s.front() < '0' || s.front() > '9') return std::unexpected(ParseErr::bad);
    int v = 0;
    auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (end != s.data() + s.size() || ec == std::errc::invalid_argument)
        return std::unexpected(ParseErr::bad);
    if (ec == std::errc::result_out_of_range || v > 65535)
        return std::unexpected(ParseErr::too_big);
    if (v < 1) return std::unexpected(ParseErr::too_small);
    return v;                                    // 值侧：隐式构造
}

const char* name(ParseErr e) {
    switch (e) {
    case ParseErr::empty:    return "empty";
    case ParseErr::bad:      return "bad";
    case ParseErr::too_small: return "too_small";
    case ParseErr::negative: return "negative";
    case ParseErr::too_big:  return "too_big";
    }
    return "?";
}

int main() {
    std::print("===== 1. PRINCIPLES =====\n");

    // 验证 1：成功/失败两个入口；仍需调用者主动检查和处理
    auto ok = parse_port("8080");
    auto bad = parse_port("-1");
    std::println("{} {} {}", ok.has_value(), ok.value(),
                 static_cast<int>(bad.error() == ParseErr::negative));
    // true 8080 1 —— bad 的错误侧是 negative

    // 验证 2：value_or 兜底拆包（错误原因被丢弃——末端手段）
    std::println("{} {}", bad.value_or(0), ok.value_or(0));   // 0 8080

    // 验证 3：*r 是"信自己"通道（不检查）；错误侧 name() 分发
    std::println("{} {}", *ok, name(bad.error()));            // 8080 negative

    std::print("===== 2. SYNTAX =====\n");

    // 2.1 单子式组合：transform 变值、and_then 接续、错误自动短路
    auto pipeline = [](std::string_view s) {
        return parse_port(s)
            .transform([](int p) { return p * 2; })        // 值侧翻倍
            .and_then([](int p) -> std::expected<int, ParseErr> {
                if (p > 16000) return std::unexpected(ParseErr::too_big);
                return p;
            });
    };
    std::println("{} {}", pipeline("7000").value_or(-1),    // 14000（通过）
                 pipeline("8080").value_or(-1));            // -1（8080*2 超
                                                            //  16000：短路）
    // 8080 在 transform 后翻倍为 16160，and_then 判超限——错误直达末端

    // 2.2 or_else：只处理错误侧（值侧原样通过）
    auto with_log = pipeline("-5").or_else(
        [](ParseErr e) -> std::expected<int, ParseErr> {
            std::println("log: parse failed ({})", name(e));
            return std::unexpected(e);                      // 记录后返回原错误，不是 throw
        });
    std::println("{} {}", with_log.has_value(),
                 static_cast<int>(with_log.error() == ParseErr::negative));
    // log: parse failed (negative)
    // false 1

    // 2.3 void 值侧：只要"成功/失败+原因"
    auto check = [](int p) -> std::expected<void, ParseErr> {
        if (p < 1024) return std::unexpected(ParseErr::too_small);
        if (p > 65535) return std::unexpected(ParseErr::too_big);
        return {};                                          // 空值侧成功
    };
    std::println("{} {}", check(80).has_value(),
                 check(8080).has_value());                   // false true

    std::print("===== 3. APPLICATION =====\n");

    // 3.1 解析流水线：错误码+out 参的现代形态
    //     旧式：bool parse_port(string_view, int& out, Err* why);
    auto show = [](std::string_view s) {
        auto r = parse_port(s);
        if (r) std::println("ok: {}", *r);
        else   std::println("err: {} ({})", name(r.error()), s);
    };
    show("443");        // ok: 443
    show("70000");      // err: too_big (70000)
    show("");           // err: empty ()
    show("8080xyz");    // err: bad (8080xyz)，不能只接受数字前缀
    show("0");          // err: too_small (0)
    show("999999999999999999999"); // err: too_big，from_chars 报范围错误而非抛出

    // 3.2 实战心法（对照 ①②③）：
    //   可预期失败 → expected + [[nodiscard]]；真异常 → 异常
    //   链上用 and_then/transform，末端才 value_or/检查
    //   E 保持轻量（enum/错误码）；error() 只在错误分支调用
    return 0;
}
