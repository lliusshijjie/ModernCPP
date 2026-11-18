// 第 56 课示例：标准库拾遗：contains、move_only_function、stacktrace（C++23）
//
// 结构：① 原理验证 —— 字符串三查；move_only_function 持独占资源
//       ② 语法演示 —— 重新绑定与置空；栈回溯捕获
//       ③ 应用实战 —— 可移动任务队列；错误上报带现场
//
// 本机（MSYS2 g++ 15.1.0）编译运行——注意链接 -lstdc++exp：
//   g++ -std=c++23 -Wall -Wextra 56_library_misc.cpp -o run -lstdc++exp && ./run
// 先预测每行输出，再运行对照！

#include <functional>   // std::move_only_function
#include <iostream>
#include <memory>
#include <print>
#include <stacktrace>
#include <string>
#include <string_view>
#include <vector>

// ③ 可移动任务队列：任务持有独占资源（function 做不到）
class TaskQueue {
public:
    void push(std::move_only_function<int()> t) {
        tasks_.push_back(std::move(t));
    }
    int run_all() {
        int sum = 0;
        for (auto& t : tasks_)
            if (t) sum += t();                       // t 可能为空（被置空）
        return sum;
    }
private:
    std::vector<std::move_only_function<int()>> tasks_;   // vector 可整体移动
};

// ③ 错误上报：抓一把现场栈（此处只打印布尔结论）
int boom(int x) {
    auto here = std::stacktrace::current();          // "出错点"抓现场
    std::println("frames>0: {}", here.size() > 0);
    return x;
}

int main() {
    std::print("===== 1. PRINCIPLES =====\n");

    // 验证 1：contains / starts_with / ends_with（string 与 string_view 都有）
    std::string url = "https://zh.cppreference.com";
    std::string_view tail = url.substr(8);           // "zh.cppreference.com"
    std::println("{} {} {}", url.contains("reference"),
                 url.starts_with("https"), url.ends_with("com"));
    // true true true
    std::println("{} {}", tail.contains("cpp"), tail.starts_with("zh"));
    // true true
    // 旧世界（对照）：url.find("reference") != std::string::npos —— 意图埋没

    // 验证 2：move_only_function 能捕获 move-only 资源
    std::move_only_function<int()> load = [blob = std::make_unique<std::string>("hi")] {
        return static_cast<int>(blob->size());
    };
    std::println("{} {}", load(), static_cast<bool>(load));   // 2 true

    // std::function 版（编译错注释保存）：
    // std::function<int()> bad = [blob = std::make_unique<std::string>("hi")] {
    //     return (int)blob->size(); };
    //   编译错：std::function 要求可拷贝构造 —— unique_ptr 捕获进不来

    std::print("===== 2. SYNTAX =====\n");

    // 2.1 重新绑定、显式置空、空检查
    load = [](void) -> int { return 7; };
    std::println("{}", load());                      // 7
    load = nullptr;                                  // 置空
    std::println("{}", static_cast<bool>(load));     // false
    load = nullptr;                                  // 再置空：合法（幂等）

    // 2.2 栈回溯：current 抓 N 层；to_string 全量描述
    auto st = std::stacktrace::current();            // 此处抓 main 一层（+内部帧）
    std::println("frames>0: {}", st.size() > 0);     // true
    std::println("desc>0: {}", !std::to_string(st).empty());   // true
    // ⚠ 本机（MinGW 原生）符号信息有限：description() 可能只给
    //   内部符号名（实测某帧为 register_frame_ctor）甚至空串；
    //   Linux(libbacktrace)/MSVC 下函数名与行号更完整——如实看待

    std::print("===== 3. APPLICATION =====\n");

    // 3.1 可移动任务队列：push 三个任务跑一遍
    TaskQueue q;
    q.push([] { return 1; });
    q.push([p = std::make_unique<int>(20)] { return *p + 1; });   // 独占捕获
    q.push([] { return 100; });
    std::println("sum={}", q.run_all());             // sum=122

    // 3.2 出错点带现场：boom 内部抓栈（只打印布尔结论，符号贫乏如实规避）
    (void)boom(5);                                   // frames>0: true

    // 3.3 实战心法（对照 ①②③）：
    //   子串判断一律 contains/starts_with/ends_with——find!=npos 退役
    //   回调要捕获独占资源/大缓冲 → move_only_function
    //   诊断信息入口 current()；符号质量看平台，别依赖行号必在
    return 0;
}
