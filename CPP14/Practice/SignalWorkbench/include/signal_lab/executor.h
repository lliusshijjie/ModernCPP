#pragma once

#include "signal_lab/model.h"

#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <thread>

namespace signal_lab {

// 小型固定工作线程池：接收分析任务；关闭时拒绝新任务、排空已接收任务并 join。
// submit 可由多个线程调用。shutdown/析构由唯一的所有者线程负责。
class Executor {
public:
    explicit Executor(std::size_t worker_count = 2);
    ~Executor();
    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;

    std::future<Report> submit(std::unique_ptr<Series> series, Settings settings);
    void shutdown();

private:
    void worker_loop();

    std::mutex mutex_;
    std::condition_variable ready_;
    // packaged_task 支持只移动 callable；std::function 要求目标可拷贝。
    std::queue<std::packaged_task<Report()>> pending_;
    std::vector<std::thread> workers_;
    bool stopping_ = false;
};

} // namespace signal_lab
