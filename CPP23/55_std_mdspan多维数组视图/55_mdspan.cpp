// 第 55 课：多维视图、步长与转置（C++23）
// g++ -std=c++23 -Wall -Wextra 55_mdspan.cpp -o run && ./run
// 有 <mdspan> 且特性宏可用时演示标准类型；否则使用教学用 MdView。
// MdView 只实现二维、动态形状、指针访问，不是 std::mdspan 的等价实现。
// 标准下标是 m[r, c]；std::submdspan 属于 C++26，本课手动构造步长映射。

#include <array>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <type_traits>
#include <vector>
#if __has_include(<mdspan>)
#include <mdspan>
#endif

#if defined(__cpp_lib_mdspan) && __cpp_lib_mdspan >= 202207L
template<class T>
using MatrixView = std::mdspan<T, std::dextents<std::size_t, 2>, std::layout_stride>;

template<class T>
MatrixView<T> make_view(T* data, std::size_t rows, std::size_t cols,
                        std::size_t row_stride, std::size_t col_stride = 1) {
    using E = std::dextents<std::size_t, 2>;
    using M = std::layout_stride::mapping<E>;
    return MatrixView<T>{data, M{E{rows, cols},
                                std::array<std::size_t, 2>{row_stride, col_stride}}};
}
#else
// 前提：步长为正且映射无重叠；所有偏移/元素总数可表示，底层存储足够。
// 断言仅作教学诊断；不负责验证存储长度或提供异常形式的边界检查。
template<class T>
class MdView {
public:
    MdView(T* data, std::size_t rows, std::size_t cols,
           std::size_t row_stride, std::size_t col_stride)
        : data_(data), rows_(rows), cols_(cols),
          row_stride_(row_stride), col_stride_(col_stride) {
        assert(row_stride > 0 && col_stride > 0);
    }
    T& operator[](std::size_t r, std::size_t c) const {
        assert(r < rows_ && c < cols_);
        return data_[r * row_stride_ + c * col_stride_];
    }
    std::size_t extent(std::size_t i) const {
        assert(i < 2);
        return i == 0 ? rows_ : cols_;
    }
    std::size_t stride(std::size_t i) const {
        assert(i < 2);
        return i == 0 ? row_stride_ : col_stride_;
    }
    std::size_t size() const { return rows_ * cols_; }
    T* data_handle() const { return data_; }
private:
    T* data_;
    std::size_t rows_, cols_, row_stride_, col_stride_;
};
template<class T>
using MatrixView = MdView<T>;

template<class T>
MatrixView<T> make_view(T* data, std::size_t rows, std::size_t cols,
                        std::size_t row_stride, std::size_t col_stride = 1) {
    return {data, rows, cols, row_stride, col_stride};
}
#endif

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";
#if defined(__cpp_lib_mdspan) && __cpp_lib_mdspan >= 202207L
    std::cout << "backend=std::mdspan\n";
#else
    std::cout << "backend=teaching MdView\n";
#endif
    std::vector<int> buf{1, 2, 3, 4, 5, 6};
    auto m = make_view(buf.data(), 2, 3, 3); // 二维行主序：偏移 r*3+c
    std::cout << m[0, 0] << ' ' << m[1, 2] << ' '
              << m.extent(0) << 'x' << m.extent(1) << '\n'; // 1 6 2x3
    // C++23 中 buf[1, 2] 不合法：vector 没有接收两个参数的 operator[]。
    // GCC 可能把旧逗号语义当扩展接受，不能据此当成标准用法。

    std::cout << "===== 2. SYNTAX =====\n";
    m[0, 1] = 20;                           // 写穿视图
    std::cout << buf[1] << '\n';            // 20
    const auto cm = m;                       // const 视图仍引用可写 int
    static_assert(std::is_same_v<decltype(cm[0, 0]), int&>);
    auto read_only = make_view(static_cast<const int*>(buf.data()), 2, 3, 3);
    static_assert(std::is_same_v<decltype(read_only[0, 0]), const int&>);
    std::cout << read_only[0, 1] << '\n';    // 20
    for (std::size_t r = 0; r < m.extent(0); ++r) {
        for (std::size_t c = 0; c < m.extent(1); ++c) std::cout << m[r, c] << ' ';
        std::cout << "| ";
    }
    std::cout << '\n';                      // 1 20 3 | 4 5 6 |
    std::cout << m.stride(0) << ' ' << m.stride(1) << '\n'; // 3 1

    // 子块跨两行、只取右两列：必须保留原来的行步长 3，不能换成子块列数 2。
    auto sub = make_view(buf.data() + 1, 2, 2, m.stride(0), m.stride(1));
    std::cout << sub[0, 0] << ' ' << sub[0, 1] << ' '
              << sub[1, 0] << ' ' << sub[1, 1] << '\n'; // 20 3 5 6
    // 真正的转置视图：形状和步长同时交换，不移动元素。
    auto transposed = make_view(buf.data(), 3, 2, m.stride(1), m.stride(0));
    std::cout << transposed[2, 1] << '\n';   // 6，与 m[1,2] 是同一元素

    std::cout << "===== 3. APPLICATION =====\n";
    for (std::size_t r = 0; r < m.extent(0); ++r) {
        int sum = 0;
        for (std::size_t c = 0; c < m.extent(1); ++c) sum += m[r, c];
        std::cout << sum << ' ';
    }
    std::cout << '\n';                      // 24 15
    std::vector<int> img{1, 2, 3, 4, 5, 6, 7, 8, 9};
    auto win = make_view(img.data(), 3, 3, 3);
    int acc = 0;
    for (std::size_t r = 0; r < 3; ++r)
        for (std::size_t c = 0; c < 3; ++c) acc += win[r, c];
    std::cout << acc << '\n';                // 45：全 1 核的窗口和
    return 0;
}
