// 第 01 课示例：泛型 lambda 与初始化捕获（C++14）
//
// 结构：① 原理验证 —— 用代码证实"lambda 就是编译器生成的匿名类"
//       ② 语法演示 —— 泛型形参 / 初始化捕获 / 多 return 推导
//       ③ 应用实战 —— 谓词复用、移动资源进任务、避免隐性拷贝
//
// 编译运行：
//   g++ -std=c++14 -Wall -Wextra 01_generic_lambda.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// ============================================================
// ① 原理验证
//    编译器为下面的泛型 lambda 生成的匿名类，结构与此手写类等价：
//    捕获 -> 成员变量；函数体 -> const 成员 operator()；auto 形参 -> 成员模板
struct Adder {
    template <class A, class B>
    auto operator()(A a, B b) const {
        return a + b;
    }
};

int main() {
    std::cout << "===== 1. PRINCIPLES =====\n";

    auto add = [](auto a, auto b) { return a + b; };

    // 验证 1：与手写模板 functor 行为一致
    static_assert(std::is_same<decltype(add(2, 3)), int>::value,
                  "add(2,3) 推导为 int");
    std::cout << add(2, 3) << ' ' << Adder{}(2, 3) << '\n';    // 5 5

    // 验证 2：每条 lambda 表达式的类型都独一无二（函数体相同也不同）
    auto add2 = [](auto a, auto b) { return a + b; };
    static_assert(!std::is_same<decltype(add), decltype(add2)>::value,
                  "两条 lambda 类型不同");
    std::cout << add2(0.5, 0.25) << '\n';                      // 0.75

    // 验证 3：观察当前实现的闭包大小；标准不保证大小恰为 1 或捕获值大小。
    int extra = 42;
    auto with_capture = [extra](auto x) { return x + extra; };
    std::cout << sizeof(decltype(add)) << ' '
              << sizeof(decltype(with_capture)) << '\n';       // 常见实现为 1 4，具体值由实现决定
    std::cout << with_capture(8) << '\n';                      // 50

    // 验证 4：无捕获 lambda 可隐式转为函数指针（闭包退化为普通函数）
    int (*fp)(int, int) = [](int a, int b) { return a + b; };
    std::cout << fp(1, 2) << '\n';                             // 3

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 auto 形参：调用点才实例化
    std::cout << add(1.5, 2) << '\n';                          // 3.5
    std::cout << add(std::string("mo"), "dern") << '\n';       // modern

    // 2.2 多条 return 自动推导；该调整也作为缺陷修正追溯到 C++11。
    auto clamp01 = [](double x) {
        if (x < 0.0) return 0.0;
        if (x > 1.0) return 1.0;
        return x;
    };
    std::cout << clamp01(2.5) << ' ' << clamp01(-1.0) << '\n'; // 1 0

    // 2.3 初始化捕获 [total = 0]：闭包自己的成员，创建时求值一次
    //     修改它必须加 mutable（operator() 默认 const）
    auto running_sum = [total = 0](auto x) mutable {
        total += x;
        return total;
    };
    std::cout << running_sum(10) << ' ' << running_sum(5) << '\n';  // 10 15

    // 2.4 初始化捕获 + std::move = 移动捕获，直接把 unique_ptr 所有权移入闭包。
    auto payload = std::make_unique<std::string>("C++14");
    auto loader  = [p = std::move(payload)] {
        return "loaded: " + *p;
    };
    std::cout << loader() << '\n';                             // loaded: C++14
    std::cout << std::boolalpha << (payload == nullptr) << '\n';    // true

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 一个泛型谓词，复用于不同元素类型的容器
    std::vector<int>    vi{3, 1, 4, 1, 5, 9, 2, 6};
    std::vector<double> vd{2.5, 1.1, 3.9};
    auto desc = [](auto a, auto b) { return a > b; };
    std::sort(vi.begin(), vi.end(), desc);
    std::sort(vd.begin(), vd.end(), desc);
    for (int n : vi) std::cout << n << ' ';
    std::cout << "| ";
    for (double d : vd) std::cout << d << ' ';
    std::cout << '\n';            // 9 6 5 4 3 2 1 1 | 3.9 2.5 1.1

    // 3.2 实战：把不可拷贝资源移进"延迟任务"（回调/线程任务同理）
    //     含 unique_ptr 的闭包只可移动 —— 进不了要求可拷贝的 std::function
    //     （经典坑！）。直接调用（或做线程入口）没问题：
    auto data = std::make_unique<std::string>("big-blob");
    auto task = [buf = std::move(data)] {
        return "process: " + *buf;
    };
    std::cout << task() << '\n';                               // process: big-blob

    //     本示例队列使用 std::function；用 shared_ptr 演示可拷贝闭包。
    //     独占任务也可以选择支持移动的包装类型，例如 std::packaged_task。
    std::vector<std::function<std::string()>> tasks;
    auto shared = std::make_shared<std::string>("shared-blob");
    tasks.push_back([buf = shared] { return "process: " + *buf; });
    tasks.push_back([] { return std::string("cleanup tmp dir"); });
    for (const auto& t : tasks) std::cout << t() << '\n';

    // 3.3 最佳实践：只读参数用 auto&&（转发引用），避免 auto 的隐性拷贝
    std::string big(1000, 'x');
    auto len_copy = [](auto s)   { return s.size(); };  // s 是整个 string 的拷贝
    auto len_fwd  = [](auto&& s) { return s.size(); };  // 绑定引用，零拷贝
    std::cout << len_copy(big) << ' ' << len_fwd(big) << '\n';   // 1000 1000

    return 0;
}
