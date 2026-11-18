// 第 55 课示例：std::mdspan 多维数组视图（C++23）
//
// 结构：① 原理验证 —— 一维存储 + 多维视图；行列主序映射
//       ② 语法演示 —— extents 静态/动态混合；切片子视图；转置
//       ③ 应用实战 —— 矩阵行和 / 卷积窗口
//
// ⚠ 本机 g++ 15.1.0（MSYS2）尚无 <mdspan> 头（libstdc++ 未实现）——
//   本文件按 std::mdspan 的公开接口手写等价物 MdView（约 40 行），
//   接口形态与标准对齐：extents / 映射公式 / operator()(i, j)。
//   换 MSVC 或含 <mdspan> 的标准库后，把 MdView 换成 std::mdspan
//   即可，用法一致。
//
// 编译运行（本课用基础库即可，不需 -lstdc++exp）：
//   g++ -std=c++23 -Wall -Wextra 55_mdspan.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <cstddef>
#include <iostream>
#include <vector>

// ---- MdView：对齐 std::mdspan<T, Extents, LayoutRight> 最小面 ----
template <typename T>
class MdView {
public:
    MdView(T* data, std::size_t rows, std::size_t cols)
        : data_(data), rows_(rows), cols_(cols) {}

    T& operator()(std::size_t r, std::size_t c) {      // 标准用 () 下标
        return data_[r * cols_ + c];                   // 行主序（layout_right）
    }
    const T& operator()(std::size_t r, std::size_t c) const {
        return data_[r * cols_ + c];
    }
    std::size_t extent(std::size_t i) const {          // 各维长度
        return i == 0 ? rows_ : cols_;
    }
    std::size_t stride(std::size_t i) const {          // 跨步（元素数）
        return i == 0 ? cols_ : 1;
    }
    std::size_t size() const { return rows_ * cols_; }
    T* data_handle() const { return data_; }           // 对应 data_handle()

private:
    T* data_;
    std::size_t rows_, cols_;
};

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：一维存储 + 多维视图（mdspan = "span + 形状 + 映射"）
    std::vector<int> buf{1, 2, 3,                      // 2 行 3 列
                         4, 5, 6};
    MdView<int> m(buf.data(), 2, 3);
    std::cout << m(0, 0) << ' ' << m(1, 2) << ' '      // 1 6（首尾元素）
              << m.extent(0) << 'x' << m.extent(1) << '\n';   // 2x3

    // 验证 2：逗号运算符陷阱——未重载多参 [] 的类型上，m[1,2] 是 m[(1,2)]
    // （编译器会警告 -Wcomma-subscript——这正是该陷阱在 C++23 被盯上的证据）
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcomma-subscript"
    std::cout << buf[1, 2] << '\n';                    // 3（buf[2]！不是"第1行第2列"）
#pragma GCC diagnostic pop
    // 逗号表达式 (1, 2) 求值取后者——想写"行 1 列 2"却拿到别的元素
    // 这正是 std::mdspan 选 operator()(1, 2) 做下标的历史原因

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 写穿视图直达底层（mdspan 是"借"，不是拷贝）
    m(0, 1) = 20;
    std::cout << buf[1] << '\n';                       // 20

    // 2.2 行主序映射：内存顺序 = 行连着放（layout_right）
    //     列主序（layout_left）则是 j * rows + i ——图像库/BLAS 两种世界
    for (std::size_t r = 0; r < 2; ++r) {
        for (std::size_t c = 0; c < 3; ++c) std::cout << m(r, c) << ' ';
        std::cout << "| ";
    }
    std::cout << '\n';                                 // 1 20 3 | 4 5 6

    // 2.3 跨步：行的相邻元素跨步 1；列的相邻元素跨步 cols
    std::cout << m.stride(0) << ' ' << m.stride(1) << '\n';   // 3 1

    // 2.4 子块视图（submdspan 的手写版）：右下 1x2
    auto sub = MdView<int>(buf.data() + 4, 1, 2);
    std::cout << sub(0, 0) << ' ' << sub(0, 1) << '\n';       // 5 6

    // 2.5 转置 = 换映射视角（不搬数据）：把 2x3 当 3x2 读
    auto transposed = [&](std::size_t r, std::size_t c) {
        return m(c, r);                                // 交换下标即转置视图
    };
    std::cout << transposed(2, 1) << '\n';             // 6（= m(1,2)）

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 行和：多维视图让循环按"语义维度"书写
    for (std::size_t r = 0; r < m.extent(0); ++r) {
        int sum = 0;
        for (std::size_t c = 0; c < m.extent(1); ++c) sum += m(r, c);
        std::cout << sum << ' ';
    }
    std::cout << '\n';                                 // 24 15（1+20+3 / 4+5+6）

    // 3.2 3x3 卷积窗口：以 (1,1) 为中心的邻域和
    //     用 3x3 数据：1 2 3 / 4 5 6 / 7 8 9
    std::vector<int> img{1, 2, 3, 4, 5, 6, 7, 8, 9};
    MdView<int> win(img.data(), 3, 3);
    int acc = 0;
    for (int dr = -1; dr <= 1; ++dr)
        for (int dc = -1; dc <= 1; ++dc)
            acc += win(1 + dr, 1 + dc);
    std::cout << acc << '\n';                          // 45（全图和）

    // 3.3 实战心法（对照 ①②③）：
    //   多维数据的"形状"是视图参数不是容器类型——一维 vector 通吃
    //   operator() 下标 + 行/列主序映射，是 mdspan 的两大支柱
    //   本机无 <mdspan>：生产可引入 kokkos-mdspan 参考实现
    return 0;
}
