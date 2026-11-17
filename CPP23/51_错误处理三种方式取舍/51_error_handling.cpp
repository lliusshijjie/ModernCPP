// 第 51 课示例：综合：异常 / 错误码 / expected 的取舍（C++23）
//
// 结构：① 原理验证 —— 同一任务三种实现并排对照
//       ② 语法演示 —— 组合传播形态的差异
//       ③ 应用实战 —— 选型清单 + 混合策略
//
// 本机（MSYS2 g++ 15.1.0）编译运行——注意链接 -lstdc++exp：
//   g++ -std=c++23 -Wall -Wextra 51_error_handling.cpp -o run -lstdc++exp && ./run
// 先预测每行输出，再运行对照！

#include <charconv>
#include <expected>
#include <print>
#include <stdexcept>
#include <string_view>
#include <system_error>

// ============ 任务：把 "8080" 解析成合法端口号（1..65535） ============

enum class ParseErr { ok = 0, bad, out_of_range, internal };
const char* name(ParseErr e) {
    switch (e) {
    case ParseErr::ok:          return "ok";
    case ParseErr::bad:         return "bad";
    case ParseErr::out_of_range: return "out_of_range";
    case ParseErr::internal:     return "internal";
    }
    return "?";
}

// ---- 实现一：异常 ----
int parse_exc(std::string_view s) {
    if (s.empty() || s.front() < '0' || s.front() > '9')
        throw std::invalid_argument("bad");
    int v = 0;
    auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (end != s.data() + s.size() || ec == std::errc::invalid_argument)
        throw std::invalid_argument("bad");
    if (ec == std::errc::result_out_of_range || v < 1 || v > 65535)
        throw std::out_of_range("out_of_range");
    return v;
}

// ---- 实现二：错误码 ----
[[nodiscard]] ParseErr parse_code(std::string_view s, int& out) {
    if (s.empty() || s.front() < '0' || s.front() > '9') return ParseErr::bad;
    int v = 0;
    auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (end != s.data() + s.size() || ec == std::errc::invalid_argument)
        return ParseErr::bad;
    if (ec == std::errc::result_out_of_range || v < 1 || v > 65535)
        return ParseErr::out_of_range;
    out = v;
    return ParseErr::ok;
}

// ---- 实现三：expected ----
[[nodiscard]] std::expected<int, ParseErr> parse_exp(std::string_view s) {
    if (s.empty() || s.front() < '0' || s.front() > '9')
        return std::unexpected(ParseErr::bad);
    int v = 0;
    auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (end != s.data() + s.size() || ec == std::errc::invalid_argument)
        return std::unexpected(ParseErr::bad);
    if (ec == std::errc::result_out_of_range || v < 1 || v > 65535)
        return std::unexpected(ParseErr::out_of_range);
    return v;
}

int main() {
    std::print("===== 1. PRINCIPLES =====\n");

    // 1.1 三种实现的调用形态（成功路径）
    int out = 0;
    auto code = parse_code("8080", out);
    if (code != ParseErr::ok) return 1;
    std::println("exc: {} code: {} exp: {}",
                 parse_exc("8080"), out, parse_exp("8080").value());
    // exc: 8080 code: 8080 exp: 8080 —— 成功时只有错误码版多占一个 out 参

    // 1.2 失败路径：异常要 try 才能拦
    try {
        (void)parse_exc("99999");             // out_of_range
    } catch (const std::exception& e) {
        std::println("caught: {}", e.what());  // caught: out_of_range
    }

    // 1.3 失败路径：错误码要记得查（忘了查 = 静默错！）
    int port = 0;
    if (auto code_err = parse_code("x", port); code_err == ParseErr::ok) {
        std::println("never here");           // （不会打印）
    } else {
        std::println("code err: {}", name(code_err));
    }
    // code err: bad

    // 1.4 失败路径：主动检查 expected；类型不会强制调用者处理
    if (auto r = parse_exp("99999")) {
        std::println("never here");
    } else {
        std::println("exp err: {}", name(r.error()));   // exp err: out_of_range
    }

    std::print("===== 2. SYNTAX =====\n");

    // 2.1 组合两层的形态：先解析再加倍，失败要穿透
    //   异常版：不加任何代码，穿透自动发生（但"会抛"是隐式契约）
    auto dbl_exc = [&](std::string_view s) {
        int p = parse_exc(s);                 // 失败：异常直接飞出去
        if (p > 30000) throw std::out_of_range("out_of_range");
        return p * 2;
    };
    try {
        (void)dbl_exc("40000");
    } catch (const std::exception& e) {
        std::println("exc pass-through: {}", e.what()); // exc pass-through: out_of_range
    }

    //   错误码版：每层都得搬运错误（样板最重）
    auto dbl_code = [&](std::string_view s, int& out2) {
        int p = 0;
        if (ParseErr e = parse_code(s, p); e != ParseErr::ok) return e;
        if (p > 30000) return ParseErr::out_of_range;
        out2 = p * 2;
        return ParseErr::ok;
    };
    int o2 = 0;
    std::println("code pass-through: {}",
                 name(dbl_code("40000", o2))); // code pass-through: out_of_range

    //   expected 版：and_then 一层，错误自动短路
    auto dbl_exp = [&](std::string_view s) {
        return parse_exp(s).and_then([](int p) -> std::expected<int, ParseErr> {
            if (p > 30000) return std::unexpected(ParseErr::out_of_range);
            return p * 2;
        });
    };
    std::println("exp pass-through: {}",
                 name(dbl_exp("40000").error()));  // exp pass-through: out_of_range

    std::print("===== 3. APPLICATION =====\n");

    // 3.1 选型速查（打印结论）
    std::println("expected: explicit value/error result and composition");
    std::println("exception: propagate failure to an agreed recovery boundary");
    std::println("err code : C ABI or project-specific status conventions");

    // 3.2 混合策略：边界收拢异常，内部用 expected
    auto safe_parse = [](std::string_view s) noexcept
        -> std::expected<int, ParseErr> {
        try {
            return parse_exc(s);              // 异常世界
        } catch (const std::invalid_argument&) {
            return std::unexpected(ParseErr::bad);
        } catch (const std::out_of_range&) {
            return std::unexpected(ParseErr::out_of_range);
        } catch (...) {
            return std::unexpected(ParseErr::internal); // 未知异常不冒充输入错误
        }
    };
    std::println("{} {}", safe_parse("443").value(),
                 name(safe_parse("zz").error()));    // 443 bad
    std::println("translated range: {}", name(safe_parse("70000").error()));
    // translated range: out_of_range（翻译时保留范围错误分类）

    // 3.3 实战心法（对照 ①②③）：
    //   根据恢复位置、API/ABI 约定与测量结果选择错误表示
    //   需要边界翻译时明确分类；noexcept 自身不会捕获异常
    //   错误码与 expected 都可加 [[nodiscard]]；检查与恢复仍由调用者负责
    return 0;
}
