// 第 44 课示例：constexpr string/vector：编译期容器（C++20）
//
// 结构：① 原理验证 —— 编译期建 vector/sort/拷出；string 拼接
//       ② 语法演示 —— 瞬态分配铁律；constexpr 变量的禁区
//       ③ 应用实战 —— 编译期单测 + 运行期双模复用
//
// 编译运行：
//   g++ -std=c++20 -Wall -Wextra 44_constexpr_containers.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

// ① 编译期容器四步曲：建 vector -> 变换 -> sort -> 拷出到 array
constexpr std::array<int, 6> sorted_squares() {
    std::vector<int> v{5, 1, 4, 1, 3};
    for (auto& x : v) x = x * x;              // constexpr 修改元素
    std::sort(v.begin(), v.end());            // constexpr 算法（C++20）
    std::array<int, 6> r{};                   // 拷出目标：字面类型
    for (std::size_t i = 0; i < v.size(); ++i) r[i] = v[i];
    return r;                                 // v 在常量求值内销毁
}                                             // —— 瞬态分配合规

// ① constexpr string：拼接、比较、取长度
constexpr std::size_t greet_len() {
    std::string a = "Hello";
    a += ", world";                           // constexpr +=
    a[0] = 'h';                               // constexpr 下标修改
    return a.size();
}

// ③ 双模函数：编译期算得出，运行期也跑得了
constexpr int count_if_const(const std::vector<int>& v, bool (*pred)(int)) {
    int n = 0;
    for (int x : v) if (pred(x)) ++n;
    return n;
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：整个计算发生在编译期——static_assert 就是单测
    static_assert(sorted_squares()[0] == 1);
    static_assert(sorted_squares()[4] == 25);
    constexpr auto sq = sorted_squares();     // 结果进 constexpr 变量
    for (int x : sq) std::cout << x << ' ';
    std::cout << '\n';                        // 1 1 9 16 25 0（拷出目标 6 格）

    // 验证 2：constexpr string 的编译期运算
    static_assert(greet_len() == 12);
    std::cout << greet_len() << '\n';         // 12

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 瞬态分配铁律（注释保存两个反面教材）：
    //   反面一：分配逃出常量求值——编译错
    //     constexpr std::vector<int>* leak() {
    //         return new std::vector<int>{1};   // new 的结果不能"活着出去"
    //     }
    //   反面二：constexpr 变量持有 vector——编译错（g++ 实测报
    //     "is not a constant expression because it refers to a
    //      result of 'operator new'"；类型上也是非字面类型）
    //     constexpr std::vector<int> v{1, 2, 3};

    // 2.2 拷出模式：vector -> array（字面类型才能进常量语境）
    constexpr auto table = [] {
        std::vector<int> fib{1, 1};
        for (int i = 2; i < 8; ++i)
            fib.push_back(fib[i - 1] + fib[i - 2]);   // constexpr push_back
        std::array<int, 8> r{};
        for (std::size_t i = 0; i < fib.size(); ++i) r[i] = fib[i];
        return r;
    }();
    static_assert(table[7] == 21);
    std::cout << table[7] << '\n';            // 21

    // 2.3 编译期与运行期的一致性：同一函数两个世界都能跑
    constexpr bool is_odd = count_if_const(std::vector<int>{1, 2, 3},
                                           [](int x) { return x % 2 == 1; })
                           == 2;
    static_assert(is_odd);
    std::vector<int> runtime_v{1, 2, 3, 4, 5};
    std::cout << count_if_const(runtime_v,
                                [](int x) { return x % 2 == 1; })
              << '\n';                        // 3（运行期实参：照常工作）

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 编译期查表：8 个 CRC8 余式进只读数据（启动零开销）
    constexpr auto crc8_table = [] {
        std::array<unsigned char, 8> t{};
        for (int i = 0; i < 8; ++i)
            t[i] = static_cast<unsigned char>((i % 2) ? 0xB8 : 0x00);
        return t;
    }();
    static_assert(crc8_table[1] == 0xB8);
    std::cout << +crc8_table[1] << '\n';      // 184

    // 3.2 实战心法（对照 ①②③）：
    //   容器在 constexpr 里是"草稿纸"：算完拷出、用完即毁
    //   static_assert = 免跑的单元测试（编译一次全验一遍）
    //   双模函数写法：编译期校验逻辑正确性，运行期复用同一实现
    return 0;
}
