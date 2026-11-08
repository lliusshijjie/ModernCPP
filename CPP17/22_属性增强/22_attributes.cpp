// 第 22 课示例：属性增强 [[nodiscard]] / [[maybe_unused]] / [[fallthrough]]（C++17）
//
// 结构：① 原理验证 —— nodiscard 类的丢弃诊断；故意丢弃的调用只列不写
//       ② 语法演示 —— nodiscard 枚举 + switch 消费；maybe_unused 形参；fallthrough
//       ③ 应用实战 —— nodiscard 结果类型 + 重试消费链
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 22_attributes.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <iostream>
#include <string>

// ① nodiscard 类：对按值返回 Check 且无意丢弃结果的调用，鼓励编译器诊断。
struct [[nodiscard]] Check {
    bool ok;
};

Check run_check(int v) { return Check{v > 0}; }

// ② nodiscard 枚举：丢弃 Status 返回值同样受保护
enum class [[nodiscard]] Status { Ok, Retry, Fail };

Status try_once(int attempt) {
    return attempt > 1 ? Status::Ok : Status::Retry;
}

// ② maybe_unused 形参：本构建不用 line —— 去掉属性，-Wextra 立即报
// "unused parameter 'line' [-Wunused-parameter]"
void log_msg(const char* msg, [[maybe_unused]] int line) {
    std::cout << msg << '\n';
}

// ② fallthrough：大小写压到同一档 —— 贯穿是语义，不是忘写 break
int tier(char c) {
    switch (c) {
        case 'a':
            [[fallthrough]];               // 声明"有意贯穿"
        case 'A':
            return 1;
        case 'b':
            [[fallthrough]];
        case 'B':
            return 2;
        default:
            return 0;
    }
}

// ③ 应用：错误处理结果类型 —— nodiscard 提醒检查，调用者仍需处理状态。
struct [[nodiscard]] ParseResult {
    bool ok;
    int value;
};

ParseResult parse_int(const std::string& s) {
    if (s.empty()) return ParseResult{false, 0};
    int v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return ParseResult{false, 0};
        v = v * 10 + (c - '0');
    }
    return ParseResult{true, v};
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：正确消费 —— 收下返回值并使用
    auto c = run_check(5);
    std::cout << c.ok << '\n';                          // true

    // 验证 2：丢弃调用会怎样？（只列不写，保持本文件零警告）
    // run_check(5);
    // warning: ignoring returned value of type 'Check' declared with
    //          attribute 'nodiscard' [-Wunused-result]

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 nodiscard 枚举 + switch(init) 消费（第 12 课组合）
    for (int i = 1; i <= 2; ++i) {
        switch (Status s = try_once(i); s) {
            case Status::Ok:
                std::cout << "ok\n";                    // 第 2 轮
                break;
            case Status::Retry:
                std::cout << "retry\n";                 // 第 1 轮
                break;
            default:
                std::cout << "fail\n";
        }
    }
    // retry
    // ok

    // 2.2 maybe_unused：line 形参未用，-Wextra 下依然零警告
    log_msg("logged", 42);                              // logged

    // 2.3 fallthrough：大小写同档
    std::cout << tier('a') << tier('B') << tier('x') << '\n';   // 120

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 nodiscard 结果类型：if-init 一条龙消费
    if (auto r = parse_int("123"); r.ok) {
        std::cout << r.value << '\n';                   // 123
    }
    auto bad = parse_int("12x");
    std::cout << bad.ok << ' ' << bad.value << '\n';    // false 0

    // 3.2 重试链：丢弃任何一环的返回值都会被编译器点名
    int attempts = 0;
    while (true) {
        Status s = try_once(++attempts);                // 消费返回值
        if (s != Status::Retry) break;
    }
    std::cout << attempts << '\n';                      // 2

    return 0;
}
