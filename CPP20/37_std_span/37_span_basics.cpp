// 第 37 课示例：std::span（C++20）
//
// 结构：① 原理验证 —— 一个接口吃下所有连续 int；就地修改；借条本质
//       ② 语法演示 —— CTAD；转换方向；切片；as_bytes
//       ③ 应用实战 —— 只读统计 + 就地修改双接口
//
// 编译运行：
//   g++ -std=c++20 -Wall -Wextra 37_span_basics.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <algorithm>
#include <array>
#include <cstddef>   // std::byte
#include <iostream>
#include <span>
#include <type_traits>
#include <utility>   // std::pair
#include <vector>

int sum(std::span<const int> s) {           // 只读借条：收一切连续 int
    int t = 0;
    for (int x : s) t += x;
    return t;
}

void bump(std::span<int> s) {               // 可写借条：就地 +1
    for (int& x : s) ++x;
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：同一函数收下 vector / std::array / C 数组
    std::vector<int> v{1, 2, 3};
    std::array<int, 3> a{4, 5, 6};
    int carr[3] = {7, 8, 9};
    std::cout << sum(v) << ' ' << sum(a) << ' ' << sum(carr) << '\n';
    // 6 15 24
    std::cout << sum(std::vector<int>{10, 20}) << '\n';    // 30（临时+只读安全）

    // 右值防护栏（对照）：
    // std::span<int> w = std::vector<int>{1, 2};   // 编译错：右值 + 非 const
    // std::span<const int> r = std::vector<int>{1, 2};  // 能编译，语句末悬垂！

    // 验证 2：span 是借条不是拷贝——写穿视图直达底层
    bump(v);
    std::cout << v[0] << v[1] << v[2] << '\n';             // 234

    // 验证 3：本实现的 span 平凡可拷贝，适合按值传参
    // C++23 起明确保证所有特化均平凡可拷贝；C++20 常见实现也满足
    static_assert(std::is_trivially_copyable_v<std::span<int>>);
    std::cout << sizeof(std::span<int>) << '\n';           // 本机 32 位目标为 8，常见 64 位实现为 16；精确布局不由标准保证

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 CTAD：array 推固定 extent；vector 推动态
    std::span fa{a};                                       // span<int, 3>
    std::span fv{v};                                       // span<int>
    static_assert(std::is_same_v<decltype(fa), std::span<int, 3>>);
    std::cout << fa.size() << ' ' << fv.size() << '\n';    // 3 3

    // 2.2 隐式转换可放宽；动态 -> 固定需显式构造并满足长度恰好一致
    std::span<int> dyn = fa;                               // OK
    std::span<const int> ro = dyn;                         // OK
    // std::span<int, 3> back = dyn;   // 编译错：不能隐式动态 -> 固定
    std::span<int, 3> back{dyn};                    // 显式合法，dyn.size() 恰好为 3
    // span 无内建 ==；比较内容用算法，不比较两个借条对象
    std::cout << std::ranges::equal(back, fa) << '\n'; // true
    std::cout << ro.size() << '\n';                        // 3

    // 2.3 切片：first / last / subspan（返回仍是借条）
    std::vector<int> w{1, 2, 3, 4, 5, 6};
    std::span<const int> sw = w;
    std::cout << sw.first(2).back() << ' '                 // 2
              << sw.last(2).front() << ' '                 // 5
              << sw.subspan(1, 2).front() << '\n';         // 2

    // 2.4 原始字节视图：as_bytes（span<const byte>）
    std::span<const std::byte> bs = std::as_bytes(sw);
    std::cout << bs.size() << ' ' << sw.size_bytes() << '\n';   // 本机 sizeof(int)==4 时为 24 24，通用值是 6*sizeof(int)

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 统计接口：只读 + 任何来源；min/max 一趟拿
    // 前提：s 非空；front() 不负责检查空区间
    auto stats = [](std::span<const int> s) {
        int lo = s.front(), hi = s.front();
        for (int x : s) { if (x < lo) lo = x; if (x > hi) hi = x; }
        return std::pair{lo, hi};
    };
    auto [lo, hi] = stats(w);
    std::cout << lo << ".." << hi << '\n';
    bump(w);                                               // 同一批数据就地 +1
    std::cout << w.front() << ' ' << w.back() << '\n';     // 2 7

    // 3.2 实战心法（对照 ①②③）：
    //   只读参数 span<const T> 按值；就地修改 span<T>
    //   借条不延寿：容器扩容/亡故后重新取 span
    //   固定 extent 编码长度契约；连续性是入场券
    return 0;
}
