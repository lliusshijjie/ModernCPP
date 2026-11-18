// 第 59 课示例：std::format 的 C++23 增强：范围与指针格式化（C++23）
//
// 结构：① 原理验证 —— 容器直接进格式串；引号规则
//       ② 语法演示 —— {:n} 无括号；map/set 形态；嵌套容器
//       ③ 应用实战 —— 调试打印一条龙；指针的本机现状
//
// 本机（MSYS2 g++ 15.1.0）编译运行——注意链接 -lstdc++exp：
//   g++ -std=c++23 -Wall -Wextra 59_format_ranges.cpp -o run -lstdc++exp && ./run
// 先预测每行输出，再运行对照！

#include <cstdint>
#include <format>
#include <iostream>
#include <map>
#include <print>
#include <set>
#include <string>
#include <vector>

int main() {
    std::print("===== 1. PRINCIPLES =====\n");

    // 验证 1：容器直接 {}——旧世界"循环打印"退役
    std::vector<int> v{1, 2, 3};
    std::print("{}\n", v);                    // [1, 2, 3]

    // 验证 2：元素类型决定引号——string 加双引号、char 加单引号
    std::print("{} {}\n",
                std::vector<std::string>{"x", "y"},   // ["x", "y"]
                std::vector<char>{'a', 'b'});         // ['a', 'b']

    // 验证 3：map 形如 JSON、set 形如数学集合
    std::map<std::string, int> m{{"a", 1}, {"b", 2}};
    std::set<int> s{7, 8};
    std::print("{} {}\n", m, s);              // {"a": 1, "b": 2} {7, 8}

    std::print("===== 2. SYNTAX =====\n");

    // 2.1 {:n} —— 去掉外层括号（csv 直出）
    std::print("{:n}\n", v);                  // 1, 2, 3
    // {:?n} 串接转义与无括号（略）——范围说明符家族：n / m / ?

    // 2.2 对齐/宽度照常作用于"整个范围"
    std::print("[{:>15}]\n", v);              // [     [1, 2, 3]]

    // 2.3 嵌套容器自然递归
    std::vector<std::vector<int>> grid{{1, 2}, {3, 4}};
    std::print("{}\n", grid);                 // [[1, 2], [3, 4]]

    // 2.4 转义形态 {:?} 与元素级说明符——本机 g++ 15 均未实现（见下方标注）

    // ⚠ 本机如实标注（g++ 15.1.0 实测，以下均为编译错、注释保存）：
    //   ① 嵌套元素级说明符尚未支持：
    //      std::print("{:.2f}", std::vector<double>{1.5});
    //      （标准允许把说明符下传给元素格式化器；实现未跟上——
    //        想控制每个元素的精度，先逐个 format 再拼）
    //   ② 转义形态 {:?} 尚未支持：
    //      std::print("{:?}", std::vector<std::string>{"a b", "c\"d"});
    //      （标准语义：["a b", "c\"d"]——引号内的引号被转义）
    //   ③ 指针的 {} 格式化（P2510）尚未支持：
    //      int* p = ...; std::print("{}", p);
    //      （下一小节给替代写法）

    std::print("===== 3. APPLICATION =====\n");

    // 3.1 调试打印一条龙：状态快照一行看清
    struct State {
        std::string name;
        std::vector<int> samples;
    };
    State st{"gpu0", {30, 45, 62}};
    std::print("state {} samples={} size={}\n",
                st.name, st.samples, st.samples.size());
    // state gpu0 samples=[30, 45, 62] size=3

    // 3.2 指针的本机替代：转 uintptr_t 打数值地址
    int x = 5;
    int* p = &x;
    std::print("non-null={} addr0x={}\n",
                p != nullptr,
                std::format("{:#x}",
                            reinterpret_cast<std::uintptr_t>(p)));
    // non-null=true addr0x=...（运行时地址，值随次变化）
    // 空指针的确定性演示：nullptr 转出来恒为 0
    std::print("null-addr={}\n",
                std::format("{:#x}",
                            reinterpret_cast<std::uintptr_t>(nullptr)));
    // null-addr=0x0

    // 3.3 实战心法（对照 ①②③）：
    //   容器打印交给 {}；引号/转义由元素类型定（调试友好）
    //   csv 用 {:n}；整体对齐宽度作用在外层
    //   本机暂不支持元素级说明符与指针 {}——用替代写法并留注释
    return 0;
}
