// 第 27 课示例：强制拷贝消除与值类别（C++17）
//
// 结构：① 原理验证 —— prvalue 直达构造（计数器实证零拷贝零移动）
//       ② 语法演示 —— prvalue 传参；xvalue 触发真实移动；参数返回走移动
//       ③ 应用实战 —— 不可移动类型的按值工厂
//
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra 27_copy_elision.cpp -o run && ./run
// 对照实验（关掉"可选"消除，NRVO 行会 +1 move；强制部分不变）：
//   g++ -std=c++17 -Wall -Wextra -fno-elide-constructors 27_copy_elision.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <iostream>
#include <utility>

// 全程计数：构造/拷贝/移动/析构各发生了几次 —— 数字不说谎
struct Traced {
    static int ctor, copy, move, dtor;
    int v;
    explicit Traced(int v) : v(v) { ++ctor; }
    Traced(const Traced& o) : v(o.v) { ++copy; }
    Traced(Traced&& o) noexcept : v(o.v) { ++move; }
    ~Traced() { ++dtor; }
    static void report(const char* tag) {
        std::cout << tag << ": ctor=" << ctor << " copy=" << copy
                  << " move=" << move << " dtor=" << dtor << '\n';
    }
};
int Traced::ctor = 0;
int Traced::copy = 0;
int Traced::move = 0;
int Traced::dtor = 0;

// prvalue 返回：强制消除 —— 调用方对象直达构造，不要求拷贝/移动构造存在
Traced make_prvalue(int v) { return Traced(v); }

// NRVO：具名局部返回 —— 可选优化（GCC 含 -O0 都做；标准不保证）
Traced make_named(int v) {
    Traced local(v);
    return local;
}

// 参数返回：无 NRVO，无消除 —— 走移动（语言对 return 参数隐式按移动处理）
Traced relay(Traced in) { return in; }

void sink(Traced x) { std::cout << "sink " << x.v << '\n'; }

// ③ 不可拷贝、不可移动 —— C++17 仍可按值返回（C++14 直接编译错误！）
struct Sealed {
    int id;
    explicit Sealed(int id) : id(id) {}
    Sealed(const Sealed&) = delete;
    Sealed(Sealed&&) = delete;
};
Sealed make_sealed(int id) { return Sealed(id); }

int main() {
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：T x = T(7) —— prvalue 配方直达 x，一次构造零拷贝零移动
    {
        Traced a = Traced(7);
        (void)a;
    }
    Traced::report("elided init");        // ctor=1 copy=0 move=0 dtor=1

    // 验证 2：prvalue 返回 —— 同样强制消除
    {
        Traced t = make_prvalue(5);
        (void)t;
    }
    Traced::report("prvalue return");     // ctor=2 copy=0 move=0 dtor=2

    // 验证 3：NRVO —— 具名局部被直接构造在调用方（GCC 做了；标准不保证）
    {
        Traced t = make_named(6);
        (void)t;
    }
    Traced::report("named NRVO");         // ctor=3 copy=0 move=0 dtor=3
    //（-fno-elide-constructors 下这行变为 move=1：NRVO 是"可选"的）

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 prvalue 实参 → 参数位置直达构造（强制消除的一部分）
    sink(Traced(3));                      // sink 3
    Traced::report("sink prvalue");       // ctor=4 copy=0 move=0 dtor=4

    // 2.2 std::move 造出 xvalue → 真实发生一次移动构造
    {
        Traced t = make_prvalue(1);       // ctor=5
        Traced u = std::move(t);          // move=1：xvalue 命中移动构造
        (void)u;
    }
    Traced::report("xvalue move");        // ctor=5 copy=0 move=1 dtor=6

    // 2.3 参数返回：无消除 —— 进参一次移动 + 返回一次移动
    {
        Traced t = make_prvalue(9);       // ctor=6
        Traced r = relay(std::move(t));   // t→in: move=2；in→返回: move=3
        (void)r;
    }
    Traced::report("relay param");        // ctor=6 copy=0 move=3 dtor=9

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 不可移动类型的按值工厂：C++17 合法、C++14 编译错误
    Sealed s = make_sealed(42);
    std::cout << "sealed " << s.id << " ok\n";   // sealed 42 ok
    // 对照：把上面的 make_sealed 用 -std=c++14 编译 ——
    //   error: use of deleted function 'Sealed::Sealed(Sealed&&)'

    // 3.2 值语义管道的底气：每层 prvalue 直达，全程零拷贝零移动
    {
        Traced final_owner = make_prvalue(make_prvalue(8).v);
        (void)final_owner;                // 外层一次直达构造；内层临时正常析构
    }
    Traced::report("pipeline");           // ctor=8 copy=0 move=3 dtor=11

    return 0;
}
