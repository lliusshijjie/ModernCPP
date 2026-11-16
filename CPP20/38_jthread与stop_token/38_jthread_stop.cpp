// 第 38 课示例：std::jthread 与 std::stop_token（C++20）
//
// 结构：① 原理验证 —— stop 三件套语义；回调执行线程
//       ② 语法演示 —— with_stop_token 入口；手动/自动收尾
//       ③ 应用实战 —— 可取消的后台 worker
//
// 编译运行：
//   g++ -std=c++20 -Wall -Wextra -pthread 38_jthread_stop.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stop_token>
#include <thread>

using namespace std::chrono_literals;   // 1ms 等（第 08 课字面量）

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：stop_source / stop_token——纯主线程即可验证语义
    std::stop_source src;
    std::stop_token tk = src.get_token();
    std::cout << tk.stop_possible() << ' ' << tk.stop_requested() << '\n';
    // true false（可停但尚未请求）
    std::cout << src.request_stop() << ' '                // true（首发）
              << src.request_stop() << ' '                // false（已停）
              << tk.stop_requested() << '\n';             // true（token 同步看见）

    // 验证 2：回调在"请求线程"同步执行；已停后再注册则立即执行
    src = std::stop_source{};                             // 换个新状态
    {
        std::stop_callback cb{src.get_token(),
                              [] { std::cout << "cb-1\n"; }};
        std::cout << "before\n";                          // before
        src.request_stop();                               // cb-1 在此行同步打印
    }
    std::cout << "register cb-2 (already stopped):\n";
    std::stop_callback cb2{src.get_token(),
                           [] { std::cout << "cb-2\n"; }};
    // 顺序：before / cb-1（请求行同步执行）/ 提示行 / cb-2（注册即执行）

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 with_stop_token 入口：首参 stop_token，jthread 自动接线
    auto worker = [](std::stop_token st) {
        while (!st.stop_requested())                      // 检查点：循环头
            std::this_thread::sleep_for(1ms);
        std::cout << "worker: done\n";
    };
    std::jthread t{worker};
    std::cout << "main: before stop\n";                   // main: before stop
    t.request_stop();
    t.join();                                             // worker: done 在 join 内
    std::cout << "main: after join\n";                    // main: after join

    // 2.2 作用域收尾：析构 = request_stop() + join()（RAII）
    {
        std::jthread t2{worker};
        std::cout << "main: scope open\n";                // main: scope open
    }                                                     // 此处自动 stop+join
    std::cout << "main: scope closed\n";                  // worker: done 先于此行

    // 2.3 显式 stop + join 后 token 视角：t 仍存活且停止状态保留
    std::cout << t.get_stop_token().stop_requested() << '\n';   // true

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 "等数据或停止"：condition_variable_any + token 的标准组合
    std::mutex m;
    std::condition_variable_any cv;
    bool ready = false;
    auto waiter = [&](std::stop_token st) {
        std::unique_lock lk(m);
        // 返回最终谓词值；数据与 stop 同时发生时，本例优先处理已就绪数据
        bool has_data = cv.wait(lk, st, [&] { return ready; });
        if (!has_data) {
            std::cout << "waiter: stopped\n";
            return;
        }
        std::cout << "waiter: got data\n";
    };

    // 路径 A：数据先到（显式 join 收尾——避免析构 stop 抢跑的竞态）
    {
        std::jthread w{waiter};
        { std::lock_guard lk(m); ready = true; }
        cv.notify_one();
        w.join();                                 // worker 醒来处理数据
    }                                             // 已 join：析构不再发 stop
    std::cout << "main: path A done\n";

    // 路径 B：谓词为 false 时请求停止；带 token 的 wait 不需要额外 notify
    {
        ready = false;
        std::jthread w{waiter};
        w.request_stop();
    }
    std::cout << "main: path B done\n";

    // 3.2 实战心法（对照 ①②③）：
    //   新代码一律 jthread；首参 stop_token 自动接线
    //   循环头/等待点是检查点；检查点密度决定取消延迟
    //   回调做短活，清理写在 worker 内部
    return 0;
}
