// 第 25 课示例：并行算法与执行策略（C++17）
//
// 结构：① 原理验证 —— 策略是重载选择 tag；seq 实跑与无策略版共存
//       ② 语法演示 —— par 的三步启用（本机无 TBB，注释给全命令）
//       ③ 应用实战 —— transform_reduce 家族与 for_each_n（顺序调用即安全）
//
// 编译运行（本机无 TBB，用默认命令）：
//   g++ -std=c++17 -Wall -Wextra 25_parallel_algorithms.cpp -o run && ./run
//
// 启用 par 的三步（MSYS2）：
//   pacman -S mingw-w64-ucrt-x86_64-tbb
//   g++ -std=c++17 -Wall -Wextra 25_parallel_algorithms.cpp -ltbb -o run && ./run
//   然后把下面 std::execution::seq 换成 std::execution::par 即可
//
// 先预测每行输出，再运行对照！

#include <algorithm>
#include <execution>
#include <functional>
#include <iostream>
#include <numeric>
#include <vector>

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：带策略重载与无策略版本共存 —— 策略是第一个参数的 tag
    std::vector<int> v{5, 3, 9, 1, 7};
    std::sort(std::execution::seq, v.begin(), v.end());      // 带策略
    for (int x : v) std::cout << x << ' ';
    std::cout << '\n';                                       // 1 3 5 7 9

    // 验证 2：策略对象本身是空 tag（零状态零开销）
    static_assert(std::is_trivially_copyable_v<decltype(std::execution::seq)>, "");

    // 验证 3：count_if 带策略 —— 语义与顺序版等价（par 是许可非强制）
    auto over4 = std::count_if(std::execution::seq, v.begin(), v.end(),
                               [](int x) { return x > 4; });
    std::cout << over4 << '\n';                              // 3

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 par 的正确写法（本机缺 TBB —— 编译可过、链接失败，故注释保存）：
    //   std::vector<int> big(10'000'000);
    //   ... 填充 ...
    //   std::sort(std::execution::par, big.begin(), big.end());        // 多线程排序
    //   auto hits = std::count_if(std::execution::par_unseq,           // + 向量化
    //                             big.begin(), big.end(), pred);
    //
    // 数据竞争的教科书反例（千万别写）：
    //   long sum = 0;
    //   std::for_each(std::execution::par, big.begin(), big.end(),
    //                 [&](int x) { sum += x; });   // UB：多线程无同步读写 sum
    // 正解见 3.1 的 transform_reduce。

    // 2.2 C++17 新算法不依赖策略：for_each_n 只处理前 n 个
    std::vector<int> taken;
    std::for_each_n(v.begin(), 3,
                    [&](int x) { taken.push_back(x * 10); });
    for (int x : taken) std::cout << x << ' ';
    std::cout << '\n';                                       // 10 30 50

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 reduce / transform_reduce 家族：并行安全的聚合（顺序调用也完全可用）
    //     注意 API 形状：纯求和是 reduce(first,last,init)；
    //     单范围 transform_reduce 必须带 (init, 归约算子, 变换算子) —— 没有 3 参版
    std::vector<int> nums{1, 2, 3, 4};
    auto total = std::reduce(nums.begin(), nums.end(), 0);
    std::cout << total << '\n';                              // 10（求和）

    // 变换后归约：先平方再求和 —— 一行替代 transform + accumulate 两趟
    auto squares = std::transform_reduce(
        nums.begin(), nums.end(), 0, std::plus{},
        [](int x) { return x * x; });
    std::cout << squares << '\n';                            // 30（1+4+9+16）

    // 3.2 两范围点积：nums·nums = 1+4+9+16（升级 par 只需在最前加策略）
    auto dot = std::transform_reduce(nums.begin(), nums.end(),
                                     nums.begin(), 0);
    std::cout << dot << '\n';                                // 30

    return 0;
}
