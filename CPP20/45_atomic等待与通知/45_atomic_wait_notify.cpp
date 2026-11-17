// 第 45 课示例：std::atomic 的等待与通知（C++20）
//
// 结构：① 原理验证 —— wait(old) 语义；丢通知免疫；无等待者通知
//       ② 语法演示 —— 真阻塞唤醒；atomic_flag 门闩
//       ③ 应用实战 —— 数据就绪握手（免锁、免条件变量）
//
// 编译运行：
//   g++ -std=c++20 -Wall -Wextra 45_atomic_wait_notify.cpp -o run && ./run
// 先预测每行输出，再运行对照！

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

int main() {
    std::cout << std::boolalpha;
    std::cout << "===== 1. PRINCIPLES =====\n";

    // 验证 1：notify 无等待者 —— 安全 no-op，不算"攒了一次通知"
    std::atomic<int> v{0};
    v.notify_one();                            // 无人可醒：直接吞掉
    std::cout << v.load() << '\n';             // 0

    // 验证 2：丢通知免疫 —— wait 先查值再睡：
    //         先改值后通知，即便通知先于 wait 发生，wait 进来见值已变立即返回
    std::atomic<int> ready{0};
    std::thread setter([&] {
        ready.store(7);                        // 先改值
        ready.notify_one();                    // 后通知（此刻可能没人等）
    });
    setter.join();                             // 改值、通知全部完成
    std::thread late_waiter([&] {
        ready.wait(0);                         // 值已是 7 != 0：立即返回不睡
        std::cout << "late waiter: passed, ready="
                  << ready.load() << '\n';
    });
    late_waiter.join();                        // late waiter: passed, ready=7

    // 验证 3：语义等价 —— wait(old) ≈ while (load() == old) 阻塞
    //         值回环（ABA）也会醒：比较的是"不等于旧值"，不是"变过几轮"
    std::atomic<int> aba{0};
    aba.store(5);
    aba.store(0);                              // 转一圈回到旧值
    // aba.wait(0) 仍会阻塞（当前值 == 0）——wait 只看当下值

    std::cout << "===== 2. SYNTAX =====\n";

    // 2.1 真阻塞唤醒：worker 睡到值变；主线程改值 + 唤醒
    std::atomic<int> signal{0};
    std::jthread worker([&](std::stop_token st) {
        signal.wait(0);                        // 阻塞（不烧 CPU）
        std::cout << "worker: signal=" << signal.load() << '\n';
        (void)st;
    });
    std::this_thread::sleep_for(5ms);          // 确保 worker 已睡下
    signal.store(42);
    signal.notify_one();                       // 唤醒
    worker.join();                             // worker: signal=42 先于此行

    // 2.2 atomic_flag：最小门闩（C++20 默认构造即"关"，ATOMIC_FLAG_INIT 已废弃）
    std::atomic_flag gate{};                   // clear 态
    gate.test_and_set();                       // 关上：返回旧值 false
    std::jthread opener([&] {
        gate.wait(true);                       // 等到"不再是 true"（有人 clear）
        std::cout << "opener: gate opened\n";
    });
    std::this_thread::sleep_for(5ms);
    gate.clear();
    gate.notify_one();
    opener.join();

    // 2.3 自由函数版（同语义，操作裸 atomic 对象地址）：
    //   std::atomic_wait(&signal, 0);
    //   std::atomic_notify_one(&signal);

    std::cout << "===== 3. APPLICATION =====\n";

    // 3.1 数据就绪握手：data + ready 两原子，免锁免条件变量
    std::atomic<int> data{0};
    std::atomic<bool> done{false};
    std::jthread consumer([&] {
        done.wait(false);                      // 等数据就绪
        std::cout << "consumer: data=" << data.load() << '\n';
    });
    data.store(99);                            // 先放数据
    done.store(true);                          // 再竖标志（顺序！）
    done.notify_one();
    consumer.join();
    std::cout << "main: consumer done\n";

    // 3.2 实战心法（对照 ①②③）：
    //   单值等待用 atomic wait；复杂谓词/多变量还得条件变量
    //   永远"先改值后通知"——丢通知免疫的前提是值先行
    //   wait 比自旋省 CPU、比条件变量省锁；但通知不计数、不排队
    return 0;
}
