// 第 10 课示例：std::tuple 按类型取元素 get<T>（C++14）—— CPP14 部分收官课
//
// 结构：① 原理验证 —— get<T> 编译期定址，与 get<I> 等价；const 类别跟随
//       ② 语法演示 —— pair/array 也支持；tie 解包
//       ③ 应用实战 —— 强类型键：类型即字段名，解决"同类型两字段"
//
// 编译运行：
//   g++ -std=c++14 -Wall -Wextra 10_tuple_get_by_type.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <array>
#include <iostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：get<T> 按类型取元素 —— 引用类别跟随 tuple 本身
    std::tuple<int, double, std::string> rec{42, 3.14, "tuple"};
    static_assert(std::is_same<decltype(std::get<int>(rec)), int&>::value, "");
    static_assert(std::is_same<decltype(std::get<std::string>(rec)),
                               std::string&>::value, "");
    std::cout << std::get<int>(rec) << '\n';          // 42
    std::cout << std::get<std::string>(rec) << '\n';  // tuple

    // 验证 2：与 get<下标> 完全等价 —— 定址都在编译期完成
    std::get<double>(rec) = 2.71;
    std::cout << std::get<1>(rec) << '\n';            // 2.71

    // 验证 3：const tuple → const 引用
    const auto& crec = rec;
    static_assert(std::is_same<decltype(std::get<int>(crec)), const int&>::value, "");
    std::cout << (std::get<int>(crec) == 42) << '\n'; // true

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 pair 按类型取（两元素类型不同时可用）
    std::pair<int, std::string> p{7, "pair"};
    std::cout << std::get<std::string>(p) << '\n';    // pair

    // 2.2 array 按下标取（编译期边界检查）
    std::array<int, 3> arr{1, 2, 3};
    std::get<2>(arr) = 9;
    std::cout << arr[2] << '\n';                      // 9

    // 2.3 tie 解包到既有变量
    int i = 0;
    double d = 0;
    std::string s;
    std::tie(i, d, s) = rec;
    std::cout << i << ' ' << d << ' ' << s << '\n';   // 42 2.71 tuple

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 强类型键：薄包装让"同底层类型"的字段也能按类型寻址
    struct Name { std::string v; };
    struct Age  { int v; };
    struct Mail { std::string v; };
    using Person = std::tuple<Name, Age, Mail>;

    Person tom{Name{"Tom"}, Age{18}, Mail{"tom@x.com"}};
    std::cout << std::get<Name>(tom).v << ' '
              << std::get<Age>(tom).v << ' '
              << std::get<Mail>(tom).v << '\n';       // Tom 18 tom@x.com

    // 3.2 按语义修改：类型即字段名，调序也不怕
    std::get<Age>(tom).v += 1;
    std::cout << std::get<Age>(tom).v << '\n';        // 19

    return 0;
}
