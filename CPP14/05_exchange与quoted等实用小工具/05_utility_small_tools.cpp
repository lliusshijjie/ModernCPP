// 第 05 课示例：std::exchange、std::quoted 等实用小工具（C++14）
//
// 结构：① 原理验证 —— exchange 三行语义；quoted 转义往返
//       ② 语法演示 —— exchange 进移动成员；_t 别名；四迭代器 equal
//       ③ 应用实战 —— quoted 字段落盘读回；exchange 取走并替换
//
// 编译运行：
//   g++ -std=c++14 -Wall -Wextra 05_utility_small_tools.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// ① exchange 的本质：三行语义（标准库实现思路）
template <class T, class U = T>
T my_exchange(T& obj, U&& new_val) {
    T old = std::move(obj);           // 偷走旧值
    obj = std::forward<U>(new_val);   // 写入新值
    return old;
}

// 移动成员样板：全部用 exchange，不可能忘记置空源
struct Buffer {
    char* data_ = nullptr;
    std::size_t size_ = 0;

    explicit Buffer(std::size_t n) : data_(new char[n]{}), size_(n) {}
    ~Buffer() { delete[] data_; }

    Buffer(const Buffer&) = delete;             // 演示聚焦移动
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& other) noexcept
        : data_(std::exchange(other.data_, nullptr)),   // 偷+置空，一行
          size_(std::exchange(other.size_, 0)) {}

    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {                   // 防自移动
            delete[] data_;                     // 先放旧资源
            data_ = std::exchange(other.data_, nullptr);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }

    std::size_t size() const { return size_; }
};

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：三行自写版与标准版行为一致
    int a = 10, b = 10;
    int old_a = my_exchange(a, 99);
    int old_b = std::exchange(b, 99);
    std::cout << old_a << ' ' << a << ' ' << old_b << ' ' << b << '\n';  // 10 99 10 99

    // 验证 2：U 可与 T 不同，走转换赋值
    double d = std::exchange(a, 3.14);       // 写入 int(3.14) = 3
    std::cout << d << ' ' << a << '\n';      // 99 3

    // 验证 3：quoted 往返 —— 引号、空格、反斜杠全部原样往返
    std::string src = "he said \"hi \\ there\" and left";
    std::ostringstream out;
    out << std::quoted(src);
    std::cout << out.str() << '\n';
    // "he said \"hi \\ there\" and left"

    std::istringstream in(out.str());
    std::string back;
    in >> std::quoted(back);
    std::cout << (back == src) << '\n';      // true

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 移动构造：两行 exchange，不可能忘记置空源
    Buffer b1(3), b2(7);
    Buffer moved = std::move(b1);
    std::cout << "moved:" << moved.size() << " b1:" << b1.size() << '\n';  // moved:3 b1:0

    // 2.2 移动赋值：先放旧资源，再偷新置空
    b2 = std::move(moved);
    std::cout << "b2:" << b2.size() << '\n';             // b2:3

    // 2.3 _t 别名家族：typename ...::type 样板消失
    using raw_t = std::remove_const_t<int* const>;       // C++11: typename std::remove_const<int* const>::type
    static_assert(std::is_same<raw_t, int*>::value, "");
    std::cout << std::is_pointer<raw_t>::value << '\n';  // true

    // 2.4 四迭代器 equal（C++14）：长度不等 → false；三迭代器老版只比前缀 → true
    std::vector<int> v1{1, 2, 3};
    std::vector<int> v2{1, 2, 3, 99};
    std::cout << std::equal(v1.begin(), v1.end(), v2.begin(), v2.end()) << '\n';  // false
    std::cout << std::equal(v1.begin(), v1.end(), v2.begin()) << '\n';            // true

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 quoted 实战：含逗号/双引号/单引号的字段安全落盘、安全读回
    std::string user = "li, \"lei\"";
    std::string city = "Xi'an";
    std::ostringstream cfg;
    cfg << std::quoted(user) << ' ' << std::quoted(city);
    std::cout << cfg.str() << '\n';          // "li, \"lei\"" "Xi'an"

    std::istringstream parse(cfg.str());
    std::string u2, c2;
    parse >> std::quoted(u2) >> std::quoted(c2);
    std::cout << (u2 == user) << ' ' << (c2 == city) << '\n';   // true true

    // 3.2 exchange 实战：取走并替换
    std::string task = "job-A";
    std::string taken = std::exchange(task, "job-B");
    std::cout << taken << ' ' << task << '\n';               // job-A job-B

    return 0;
}
