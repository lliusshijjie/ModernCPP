// 第 59 课：C++23 范围格式化与已有的指针格式化
// g++ -std=c++23 -Wall -Wextra 59_format_ranges.cpp -o run -lstdc++exp && ./run
// 元素说明符前要额外一个冒号。int* 不能直接格式化地址，显式转 const void*。
#include <format>
#include <map>
#include <print>
#include <set>
#include <string>
#include <vector>

int main() {
    std::print("===== 1. PRINCIPLES =====\n");
    std::vector<int> v{1, 2, 3};
    std::print("{}\n", v);                            // [1, 2, 3]
    std::print("{} {}\n", std::vector<std::string>{"x", "y"},
                           std::vector<char>{'a', 'b'}); // ["x", "y"] ['a', 'b']
    std::map<std::string, int> m{{"a", 1}, {"b", 2}};
    std::set<int> s{7, 8};
    std::print("{} {}\n", m, s);                       // {"a": 1, "b": 2} {7, 8}
    // 这是调试表示，不是对任意类型都有效的 JSON 序列化。

    std::print("===== 2. SYNTAX =====\n");
    std::print("{:n}\n", v);                          // 1, 2, 3：只去括号，不保证 CSV 转义
    std::print("[{:>15}]\n", v);                      // [      [1, 2, 3]]：六个填充空格
    std::print("{::>4}\n", v);                        // [   1,    2,    3]：每个元素宽 4
    std::print("{::.2f}\n", std::vector<double>{1.5, 2.0}); // [1.50, 2.00]
    std::vector<std::vector<int>> grid{{1, 2}, {3, 4}};
    std::print("{}\n", grid);                         // [[1, 2], [3, 4]]

    std::vector<std::string> words{"a b", "c\"d"};
    std::print("{}\n", words);                        // ["a b", "c\"d"]：默认已转义
    std::print("{::?}\n", words);                     // 同上：显式指定元素调试格式
    std::print("{::}\n", words);                      // [a b, c"d]：显式空元素格式取消默认引号
    std::vector<char> text{'a', '\n'};
    std::print("{:?s}\n", text);                      // "a\n"：整个字符范围作为转义字符串
    // {:?} 适用于单个字符串/字符，不是任意范围的说明符；范围可用 ?s 或 ::?。

    std::print("===== 3. APPLICATION =====\n");
    struct State { std::string name; std::vector<int> samples; };
    State st{"gpu0", {30, 45, 62}};
    std::print("state {} samples={} size={}\n", st.name, st.samples, st.samples.size());
    // state gpu0 samples=[30, 45, 62] size=3
    int x = 5;
    int* p = &x;
    std::print("non-null={} addr={}\n", p != nullptr, static_cast<const void*>(p));
    std::print("null-addr={}\n", nullptr);             // 本机为 0x0；表示依实现而定
    // const char* 的 {} 打印字符串内容；需要它的地址时也应转为 const void*。
    // P2510R3 是 C++26 的补零/大写 P 扩展，不是允许任意 T* 的提案；实现可能回移。
    return 0;
}
