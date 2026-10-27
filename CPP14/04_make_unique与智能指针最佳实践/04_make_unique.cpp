// 第 04 课示例：std::make_unique 与智能指针最佳实践（C++14）
//
// 结构：① 原理验证 —— make_unique 无魔法 / 零开销 / 异常路径对比泄漏
//       ② 语法演示 —— 完美转发 / 数组形式 / move-only / 类型写一遍
//       ③ 应用实战 —— 工厂返回 unique_ptr（可升级 shared）/ 容器接管所有权
//
// 编译运行：
//   g++ -std=c++14 -Wall -Wextra 04_make_unique.cpp -o run && ./run
// 先预测每行输出（含 ctor/dtor 的出现位置！），再运行对照！

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// 演示用类型：构造/析构都喊一声，所有权流转一目了然
struct Widget {
    int v;
    explicit Widget(int v) : v(v) { std::cout << "Widget(" << v << ") ctor\n"; }
    ~Widget() { std::cout << "Widget(" << v << ") dtor\n"; }
};

// ① 原理验证 1：make_unique 没有魔法 —— 这就是它的全部实现思路
template <class T, class... Args>
std::unique_ptr<T> my_make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// 异常路径对比：unique_ptr 兜底 vs 裸 new 泄漏
void safe_path() {
    auto p = std::make_unique<Widget>(1);    // RAII：析构由作用域保证
    throw std::runtime_error("boom");
}

void leak_path() {
    Widget* raw = new Widget(2);             // 无人接管
    (void)raw;                               // 演示用：假装后面会 delete
    throw std::runtime_error("boom");        // raw 永远等不到 delete —— 泄漏
}

std::unique_ptr<Widget> make_widget(int v) { // ③ 工厂：默认返回 unique_ptr
    return std::make_unique<Widget>(v);
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：语义与"new + 接管"完全一致
    auto w = my_make_unique<Widget>(5);      // Widget(5) ctor
    std::cout << w->v << '\n';               // 5

    // 验证 2：拷贝确实被删除（move-only 是编译期契约）
    static_assert(!std::is_copy_constructible<std::unique_ptr<Widget>>::value,
                  "unique_ptr is move-only");

    // 验证 3：默认删除器被 EBO 吃掉 —— 独占指针是裸指针的价格
    std::cout << sizeof(std::unique_ptr<Widget>) << ' '
              << sizeof(Widget*) << '\n';    // 8 8

    // 验证 4：异常路径对比 —— 看 dtor 出不出现
    try {
        safe_path();                         // Widget(1) ctor → 栈展开 → dtor
    } catch (const std::exception& e) {
        std::cout << "caught: " << e.what() << '\n';
    }
    try {
        leak_path();                         // Widget(2) ctor → 没有 dtor！
    } catch (const std::exception& e) {
        std::cout << "caught: " << e.what() << '\n';
    }

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 完美转发：直接把参数转给 T 的构造函数
    auto a = std::make_unique<std::string>(3, 'x');
    std::cout << *a << '\n';                 // xxx

    // 2.2 数组形式（C++14 新增）：值初始化，元素全为 0
    auto arr = std::make_unique<int[]>(4);
    std::cout << arr[0] << arr[3] << '\n';   // 00

    // 2.3 move-only：转移后源指针为空（没有新对象被构造）
    auto src = std::make_unique<Widget>(7);  // Widget(7) ctor
    auto dst = std::move(src);
    std::cout << dst->v << ' ' << (src == nullptr) << '\n';   // 7 true

    // 2.4 类型只写一遍（对比：std::unique_ptr<std::vector<int>> v(new ...)）
    auto b = std::make_unique<std::vector<int>>(3, 9);
    std::cout << b->size() << ' ' << (*b)[0] << '\n';         // 3 9

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 工厂返回 unique_ptr；要共享的客户自行隐式升级
    auto w1 = make_widget(10);               // Widget(10) ctor
    std::shared_ptr<Widget> w2 = make_widget(11);   // Widget(11) ctor，升级
    std::cout << w1->v << ' ' << w2->v << ' '
              << w2.use_count() << '\n';     // 10 11 1

    // 3.2 容器接管所有权（push_back 右值，无需显式 move）
    std::vector<std::unique_ptr<Widget>> v;
    v.push_back(std::make_unique<Widget>(20));   // Widget(20) ctor
    v.push_back(make_widget(21));            // Widget(21) ctor
    std::cout << v.size() << ' ' << v[0]->v << ' ' << v[1]->v << '\n';   // 2 20 21

    return 0;
    // main 结束：自动变量按声明逆序析构；vector 元素按存储顺序析构（先 v[0] 后 v[1]）
    // —— 注意观察下面 6 行 dtor 的顺序，两种"逆序/正序"别搞混：
    // Widget(20) dtor   ← v[0]（vector 正序）
    // Widget(21) dtor   ← v[1]
    // Widget(11) dtor   ← w2 (shared_ptr)
    // Widget(10) dtor   ← w1
    // Widget(7)  dtor   ← dst
    // Widget(5)  dtor   ← w（①里最早构造，所以最后析构——自动变量逆序）
}
