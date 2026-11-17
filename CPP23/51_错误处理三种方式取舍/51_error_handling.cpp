// 第 51 课示例：综合：异常 / 错误码 / expected 的取舍（C++23）
//
// 结构：① 原理验证 —— 同一任务三种实现并排对照
//       ② 语法演示 —— 组合传播形态的差异
//       ③ 应用实战 —— 选型清单 + 混合策略
//
// 本机（MSYS2 g++ 15.1.0）编译运行——注意链接 -lstdc++exp：
//   g++ -std=c++23 -Wall -Wextra 51_error_handling.cpp -o run -lstdc++exp && ./run
// 先预测每行输出，再运行对照！

#include <expected>
#include <iostream>
#include <print>
#include <string>
#include <string_view>

// ============ 任务：把 "8080" 解析成合法端口号（1..65535） ============

enum class ParseErr { ok, bad, out_of_range };   // ok 是显式成功值！
const char* name(ParseErr e) {
    switch (e) {
    case ParseErr::ok:          return "ok";
    case ParseErr::bad:         return "bad";
    case ParseErr::out_of_range: return "out_of_range";
    }
    return "?";
}

// ---- 实现一：异常 ----
int parse_exc(std::string_view s) {
    if (s.empty() || s[0] == '-') throw std::invalid_argument("bad");
    int v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') throw std::invalid_argument("bad");
        v = v * 10 + (c - '0');
        if (v > 65535) throw std::out_of_range("out_of_range");
    }
    return v;
}

// ---- 实现二：错误码 ----
ParseErr parse_code(std::string_view s, int& out) {
    if (s.empty()) return ParseErr::bad;
    int v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return ParseErr::bad;
        v = v * 10 + (c - '0');
        if (v > 65535) return ParseErr::out_of_range;
    }
    out = v;
    return ParseErr::ok;
}

// ---- 实现三：expected ----
std::expected<int, ParseErr> parse_exp(std::string_view s) {
    int v = 0;
    if (!s.empty())
        for (char c : s) {
            if (c < '0' || c > '9') return std::unexpected(ParseErr::bad);
            v = v * 10 + (c - '0');
            if (v > 65535) return std::unexpected(ParseErr::out_of_range);
        }
    else return std::unexpected(ParseErr::bad);
    return v;
}

int main() {
    std::print("===== 1. PRINCIPLES =====\n");

    // 1.1 三种实现的调用形态（成功路径）
    int out = 0;
    std::println("exc: {} code: {} exp: {}",
                 parse_exc("8080"),
                 (parse_code("8080", out), out),
                 parse_exp("8080").value());
    // exc: 8080 code: 8080 exp: 8080 —— 成功时只有错误码版多占一个 out 参

    // 1.2 失败路径：异常要 try 才能拦
    try {
        (void)parse_exc("99999");             // out_of_range
    } catch (const std::exception& e) {
        std::println("caught: {}", e.what());  // caught: out_of_range
    }

    // 1.3 失败路径：错误码要记得查（忘了查 = 静默错！）
    int port = 0;
    if (parse_code("x", port) == ParseErr{}) {
        std::println("never here");           // （不会打印）
    } else {
        std::println("code err: {}", name(parse_code("x", port)));
    }
    // code err: bad

    // 1.4 失败路径：expected 的拆包被类型系统盯着
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
        if (ParseErr e = parse_code(s, p); e != ParseErr{}) return e;
        if (p > 30000) return ParseErr::out_of_range;
        out2 = p * 2;
        return ParseErr{};
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
    std::println("expected: parse, lookup, validate (flow, checkable)");
    std::println("exception: invariant break, alloc fail (rare, propagate)");
    std::println("err code : C ABI, hot path, embedded (cheapest)");

    // 3.2 混合策略：边界收拢异常，内部用 expected
    auto safe_parse = [](std::string_view s) noexcept
        -> std::expected<int, ParseErr> {
        try {
            return parse_exc(s);              // 异常世界
        } catch (...) {
            return std::unexpected(ParseErr::bad);   // 翻译成值
        }
    };
    std::println("{} {}", safe_parse("443").value(),
                 name(safe_parse("zz").error()));    // 443 bad

    // 3.3 实战心法（对照 ①②③）：
    //   可预期失败 expected；不可预期异常；跨语言/极热路径错误码
    //   模块边界把异常翻译成 expected（noexcept 包装），错误不越界
    //   忘查检查：错误码致命、optional 弱、expected+[[nodiscard]] 强
    return 0;
}
