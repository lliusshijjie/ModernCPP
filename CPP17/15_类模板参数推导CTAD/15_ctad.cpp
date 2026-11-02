// 第 15 课示例：类模板参数推导 CTAD（C++17）
//
// 结构：① 原理验证 —— 构造实参直接决定模板参数；拷贝推导
//       ② 语法演示 —— vector/lock_guard 的显式指引；混型歧义（只列不写）
//       ③ 应用实战 —— 自定义类的隐式与显式指引
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 15_ctad.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <initializer_list>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// ③ 隐式指引：initializer_list 构造函数自动合成
template <class T>
struct MyVec {
    std::vector<T> v;
    MyVec(std::initializer_list<T> il) : v(il) {}
    std::size_t size() const { return v.size(); }
};

// ③ 显式推导指引：Fill(长度, 值) -> Fill<T>
template <class T>
struct Fill {
    std::vector<T> v;
    Fill(std::size_t n, T val) : v(n, val) {}
};
template <class T>
Fill(std::size_t, T) -> Fill<T>;

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：构造实参直接决定模板参数 —— 不再需要 make_pair
    std::pair p1{1, 2.5};
    static_assert(std::is_same_v<decltype(p1), std::pair<int, double>>, "");
    std::cout << p1.first << ' ' << p1.second << '\n';    // 1 2.5

    // 验证 2：tuple 同理
    std::tuple t1(1, 2.5, 'c');
    static_assert(std::is_same_v<decltype(t1), std::tuple<int, double, char>>, "");
    std::cout << std::get<0>(t1) << ' ' << std::get<char>(t1) << '\n';    // 1 c

    // 验证 3：拷贝推导 —— 从同类实例沿用模板参数
    std::vector v1{1, 2, 3};
    std::vector v2 = v1;
    static_assert(std::is_same_v<decltype(v2), std::vector<int>>, "");
    std::cout << v2.size() << '\n';                       // 3

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 vector 经标准库显式指引推导
    std::vector v{1, 2, 3};                // vector<int>
    std::cout << v.size() << ' ' << v[2] << '\n';         // 3 3

    std::vector w{1.5, 2.5};               // vector<double>
    std::cout << w[0] + w[1] << '\n';                     // 4

    // 2.2 lock_guard 的指引 —— RAII 少敲一串类型
    std::mutex mtx;
    {
        std::lock_guard lg(mtx);           // lock_guard<std::mutex>
        std::cout << "locked\n";           // locked
    }

    // 2.3 混型大括号列表推导失败（歧义保护，只列不写）：
    // std::vector bad{1, 2.5};           // error: 推导冲突

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 隐式指引：initializer_list 构造函数自动合成
    MyVec mv{10, 20, 30};                  // MyVec<int>
    std::cout << mv.size() << '\n';        // 3

    // 3.2 显式推导指引：实参里的 T 决定 Fill<T>
    Fill f(3, std::string("x"));           // Fill<std::string>
    std::cout << f.v.size() << ' ' << f.v[0] << '\n';     // 3 x

    // 3.3 实战惯用：braced 实参直接进 insert
    std::map<std::string, int> m;
    m.insert({"k", 1});
    std::cout << m.begin()->second << '\n';               // 1

    return 0;
}
