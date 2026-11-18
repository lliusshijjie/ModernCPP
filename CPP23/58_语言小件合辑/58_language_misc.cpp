// 第 58 课：if consteval、static operator()、多维 operator[]（C++23）
// g++ -std=c++23 -Wall -Wextra 58_language_misc.cpp -o run -lstdc++exp && ./run
#include <bit>
#include <cstddef>
#include <print>
#include <type_traits>

// 两条路径计算相同的结果。popcount 本身从 C++20 起就能 constexpr，
// 此处手写分支仅用于展示分派；不是说必须为 popcount 另写编译期实现。
constexpr int count_bits(unsigned x) {
    if consteval {
        int count = 0;
        while (x != 0) {
            count += static_cast<int>(x & 1u);
            x >>= 1;
        }
        return count;
    } else {
        return std::popcount(x);
    }
}

consteval unsigned immediate_plus_one(unsigned x) { return x + 1; }
constexpr unsigned plus_one(unsigned x) {
    if consteval {
        // 真分支是 immediate function context，允许参数 x 传给 consteval 函数。
        return immediate_plus_one(x);
    } else {
        return x + 1;
    }
}

struct Square {
    static constexpr double operator()(double x) { return x * x; }
};
static_assert(std::is_same_v<decltype(&Square::operator()), double(*)(double)>);

struct Mat2x2 {
    double m[4] = {1, 2, 3, 4};
    // 本例不查边界；重载本身完全可以加入检查并抛出异常。
    double& operator[](std::size_t r, std::size_t c) { return m[r * 2 + c]; }
    const double& operator[](std::size_t r, std::size_t c) const { return m[r * 2 + c]; }
};

int main() {
    std::print("===== 1. PRINCIPLES =====\n");
    static_assert(count_bits(11u) == 3);
    std::print("{} {}\n", count_bits(11u), count_bits(15u)); // 3 4：普通调用走运行期分支
    static_assert(plus_one(41) == 42);
    std::print("{}\n", plus_one(41));                       // 42
    // 普通 if (std::is_constant_evaluated()) 也合法，但不创建立即函数上下文。
    // if constexpr (std::is_constant_evaluated()) 的条件恒为 true，是另一回事。

    Square s{};
    std::print("{} {}\n", s(1.5), Square{}(2.5));           // 2.25 6.25
    // static 不意味着 consteval；普通非 static operator() 也早就能 constexpr。

    std::print("===== 2. SYNTAX =====\n");
    Mat2x2 a;
    a[1, 0] = 30;
    std::print("{} {} {}\n", a[0, 0], a[1, 0], a[1, 1]);  // 1 30 4
    double raw[4] = {1, 2, 3, 4};
    // raw[1, 2] 在标准 C++23 中不合法：内建下标只接收一个参数。
    // 更早版本的逗号表达式要保留含义可显式写 raw[(left, right)]，但通常应改成：
    std::print("{}\n", raw[2]);                             // 3
    static_assert(Square{}(3.0) == 9.0);
    std::print("static assert ok\n");

    std::print("===== 3. APPLICATION =====\n");
    struct ByTwice { static int operator()(int x) { return x * 2; } };
    struct BySquare { static int operator()(int x) { return x * x; } };
    auto apply = [](auto op, int v) { return op(v); };
    std::print("{} {}\n", apply(ByTwice{}, 21), apply(BySquare{}, 5)); // 42 25
    // static 调用没有隐式对象参数；obj 表达式仍会求值，不保证比非 static 更快。
    return 0;
}
