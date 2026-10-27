// 第 03 课示例：变量模板与 std::integer_sequence（C++14）
//
// 结构：① 原理验证 —— 变量模板是"变量家族"；整数序列的数据在类型里
//       ② 语法演示 —— 全特化/偏特化/_v 惯用法/序列展开三件套
//       ③ 应用实战 —— 元组展开（print / 自制 apply）；类型级常量
//
// 编译运行：
//   g++ -std=c++14 -Wall -Wextra 03_variable_template.cpp -o run && ./run
// 先预测每行输出，再运行对照！
//
// （GCC/Clang 专有：__PRETTY_FUNCTION__ 用于"打印"模板实参类型）

#include <cstddef>
#include <iostream>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

// ---- 变量模板：基本形式，float/double/int 各得其所 ----
template <class T>
constexpr T pi_t = T(3.141592653589793238462643L);

template <>                       // 全特化：int 版本只要 3
constexpr int pi_t<int> = 3;

// ---- 变量模板偏特化：当"类型谓词"用 ----
template <class T>
constexpr bool is_ptr_v = false;

template <class T>
constexpr bool is_ptr_v<T*> = true;

// ---- _v 惯用法：消灭 traits 的 ::value 拖尾 ----
template <class T, class U>
constexpr bool is_same_v = std::is_same<T, U>::value;

// ---- 类型级编译期常量（③-3 用）----
template <class T>
constexpr T kEpsilon = std::numeric_limits<T>::epsilon();

// ---- integer_sequence 取用三件套（② 用）----
template <class T, T... Is>
constexpr std::size_t seq_len(std::integer_sequence<T, Is...>) {
    return sizeof...(Is);
}

template <class T, T... Is>
constexpr T seq_sum(std::integer_sequence<T, Is...>) {
    const T arr[] = { Is... };        // 包展开落进数组初始化列表
    T s = 0;
    for (T v : arr) s += v;
    return s;
}

// ---- GCC/Clang 专有：把模板实参"打印"出来（① 用）----
template <class T>
constexpr const char* type_name() {
    return __PRETTY_FUNCTION__;
}

// ---- ③ 应用实战：元组展开 ----
// 3.1 打印 tuple：C++14 没有结构化绑定/折叠表达式，index_sequence 是标准手法
template <class Tuple, std::size_t... Is>
void print_tuple_impl(const Tuple& t, std::index_sequence<Is...>) {
    int dummy[] = { 0, ((std::cout << (Is == 0 ? "" : ", ")
                                    << std::get<Is>(t)), 0)... };
    (void)dummy;
}

template <class... Ts>
void print_tuple(const std::tuple<Ts...>& t) {
    std::cout << "[";
    print_tuple_impl(t, std::index_sequence_for<Ts...>{});
    std::cout << "]\n";
}

// 3.2 模拟 std::apply（C++17 才有）：把元组解包成函数实参
template <class F, class Tuple, std::size_t... Is>
auto apply_impl(F f, const Tuple& t, std::index_sequence<Is...>)
    -> decltype(f(std::get<Is>(t)...)) {
    return f(std::get<Is>(t)...);
}

template <class F, class... Ts>
auto my_apply(F f, const std::tuple<Ts...>& t)
    -> decltype(f(std::declval<Ts>()...)) {
    return apply_impl(f, t, std::index_sequence_for<Ts...>{});
}

int main() {
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：make_index_sequence<N> 生成的就是字面类型 integer_sequence<size_t, 0..N-1>
    static_assert(std::is_same<
        std::make_index_sequence<3>,
        std::integer_sequence<std::size_t, 0, 1, 2>>::value, "");

    // 验证 2：数据全在类型里 —— 对象是空壳，1000 个索引和 3 个索引一样大
    std::cout << sizeof(std::make_index_sequence<1000>) << ' '
              << sizeof(std::make_index_sequence<3>) << '\n';       // 1 1

    // 验证 3：亲眼看一下这个类型（GCC/Clang 专有）
    std::cout << type_name<std::make_index_sequence<3>>() << '\n';
    // constexpr const char* type_name() [with T = std::integer_sequence<long long unsigned int, 0, 1, 2>]

    // 验证 4：变量模板 = 变量家族，pi_t<float>/pi_t<double>/pi_t<int> 是独立实体
    std::cout << pi_t<float> << ' ' << pi_t<double> << ' '
              << pi_t<int> << '\n';                                 // 3.14159 3.14159 3

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 偏特化谓词：非指针命中 false
    std::cout << is_ptr_v<double> << '\n';                          // 0

    // 2.2 指针（含"指向 const 的指针"）命中 true
    std::cout << is_ptr_v<int> << ' ' << is_ptr_v<int*> << ' '
              << is_ptr_v<const double*> << '\n';                   // 0 1 1

    // 2.3 _v 惯用法：C++11 要写 std::is_same<int,int>::value
    std::cout << std::boolalpha << is_same_v<int, int> << ' '
              << is_same_v<int, long> << '\n';                      // true false

    // 2.4 序列三件套：形参推导出 Is... / sizeof... 数个数 / 展开进数组求和
    std::cout << seq_len(std::integer_sequence<int, 7, 8, 9>{}) << ' '
              << seq_sum(std::integer_sequence<int, 7, 8, 9>{})
              << '\n';                                              // 3 24

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 展开元组打印
    auto t = std::make_tuple(1, 2.5, std::string("cpp"));
    print_tuple(t);                                                 // [1, 2.5, cpp]

    // 3.2 元组解包成函数实参（自制 apply）
    int total = my_apply([](int a, double b, const std::string& s) {
        return a + int(b) + int(s.size());    // 1 + 2 + 3
    }, t);
    std::cout << total << '\n';                                     // 6

    // 3.3 类型级编译期常量
    std::cout << kEpsilon<float> << ' ' << kEpsilon<double> << '\n';
    // 1.19209e-07 2.22045e-16

    return 0;
}
