// 补充 28A：运算符写法、显式函数调用和列表初始化的定序边界。
#include <array>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>

struct Sink {
    std::string* trace;
};

Sink make_sink(std::string& trace) {
    trace += 'L';
    return {&trace};
}

int make_value(std::string& trace) {
    trace += 'R';
    return 7;
}

Sink operator<<(Sink sink, int value) {
    if (value != 7) throw std::runtime_error("unexpected value");
    *sink.trace += 'C';
    return sink;
}

int mark(std::string& trace, char label, int value) {
    trace += label;
    return value;
}

struct Reader {
    std::array<int, 2> fields{17, 3};
    std::size_t next = 0;
    int read() {
        const int value = fields.at(next);
        ++next;
        return value;
    }
};

struct Request {
    int item;
    int count;
};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        std::cout << std::boolalpha;
        std::cout << "===== 1. PRINCIPLES =====\n";
        std::string trace;
        (void)(make_sink(trace) << make_value(trace));
        require(trace == "LRC", "operator notation must evaluate left first");
        std::cout << "operator notation: " << trace << '\n'; // LRC

        trace.clear();
        (void)operator<<(make_sink(trace), make_value(trace)); // 普通函数调用写法。
        const bool allowed = trace == "LRC" || trace == "RLC";
        require(allowed, "unexpected function-argument order");
        std::cout << "function notation: " << trace << '\n'; // LRC 或 RLC，均合法。
        std::cout << "allowed order: " << allowed << '\n';   // true

        std::cout << "===== 2. SYNTAX =====\n";
        trace.clear();
        const std::array<int, 3> values{
            mark(trace, 'A', 1), mark(trace, 'B', 2), mark(trace, 'C', 3)};
        require(trace == "ABC" && values[0] == 1 && values[2] == 3,
                "list-initialization order failed");
        std::cout << "braced list: " << trace << '\n'; // ABC；此规则在 C++11 已有。

        std::cout << "===== 3. APPLICATION =====\n";
        Reader input;
        // read() 会推进游标；按协议字段顺序分别求值，再传递普通值。
        const int item = input.read();
        const int count = input.read();
        const Request request{item, count};
        require(request.item == 17 && request.count == 3 && input.next == 2,
                "request field order failed");
        std::cout << "request: " << request.item << ' ' << request.count << '\n'; // 17 3
        std::cout << "checks: PASS\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
