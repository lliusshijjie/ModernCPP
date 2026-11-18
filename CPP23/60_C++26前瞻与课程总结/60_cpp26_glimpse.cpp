// 第 60 课姊妹文件：C++26 一瞥（本机 g++ 15.1.0 实测可用部分）
//
// 需要用 C++26 标准编译（print 仍需链接 -lstdc++exp，系列惯例）：
//   g++ -std=c++26 -Wall -Wextra 60_cpp26_glimpse.cpp -o run26 -lstdc++exp && ./run26
//
// 收录两项（均实测通过）：包索引 pack indexing 与 =delete("原因")

#include <print>
#include <type_traits>

// ① 包索引（P2662）：Ts...[I] 取类型包的第 I 个；args...[I] 取值包
template <class... Ts>
using first_t = Ts...[0];

template <class... Ts>
auto first_arg(Ts... args) {
    return args...[0];
}

// ② =delete 带原因（P2573）：报错信息随签名携带
void set_port(int);
void set_port(int) { /* 本体 */ }
void set_port(double) = delete("port must be int; use set_port(int)");

// ③ 已标准化但本机 g++ 15 未实现（如实标注，仅列清单）：
//   - 反射（P2996）：reify/info 一族，模板元编程的换代
//   - 契约（P2900）：pre/post/contract_assert 运行时检查设施
//   - std::execution（P2300）：结构化异步（senders/receivers）
//   - hazard_pointer / rcu（P2530/P2545）：无锁读取原语
//   - std::inplace_vector（P0843）：栈上定长 vector

int main() {
    // ① 包索引：第 03 课 integer_sequence 时代的技巧（递归/特化
    //    取首个类型）一行化
    static_assert(std::is_same_v<first_t<int, double, char>, int>);
    std::print("first int? {}\n",
                std::is_same_v<first_t<int, double, char>, int>);   // true
    std::print("first arg = {}\n", first_arg(7, 8, 9));             // 7

    // ② 误用被拒时，编译器会引用我们的原因字符串（调用处注释验证）：
    set_port(8080);            // OK
    // set_port(80.5);         // 编译错：候选被 delete，报错含
    //                          // "port must be int; use set_port(int)"
    std::print("set_port(8080) ok\n");

    std::print("C++26: more to come (reflection, contracts, execution)\n");
    return 0;
}
