#include "signal_lab/executor.h"

namespace signal_lab {

Executor::Executor(std::size_t worker_count) {
    if (worker_count == 0 || worker_count > 4) {
        throw std::invalid_argument("worker count must be between 1 and 4");
    }
    workers_.reserve(worker_count);
    try {
        for (std::size_t i = 0; i < worker_count; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    } catch (...) {
        // 部分线程已创建时也必须通知并 join，不能让 joinable thread 被直接析构。
        shutdown();
        throw;
    }
}

Executor::~Executor() { shutdown(); }

std::future<Report> Executor::submit(std::unique_ptr<Series> series, Settings settings) {
    if (!series) throw std::invalid_argument("cannot submit a null series");
    validate_settings(settings);
    std::unique_lock<std::mutex> lock(mutex_);
    if (stopping_) throw std::logic_error("executor is closed");
    if (pending_.size() >= 32) throw std::length_error("pending queue is full");

    // [01/04] 快照移入闭包；闭包不再依赖 submit 调用栈中的局部变量。
    // [03A] decay_t 可用于命名闭包的实际类型；目标是只可移动类型。
    auto operation = [owned = std::move(series), options = std::move(settings)] {
        return analyze(*owned, options);
    };
    static_assert(!std::is_copy_constructible<std::decay_t<decltype(operation)>>::value,
                  "the task must own its unique snapshot");
    std::packaged_task<Report()> task(std::move(operation));
    auto result = task.get_future();
    pending_.push(std::move(task));
    lock.unlock();
    ready_.notify_one();
    return result;
}

void Executor::worker_loop() {
    for (;;) {
        std::packaged_task<Report()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ready_.wait(lock, [this] { return stopping_ || !pending_.empty(); });
            if (pending_.empty()) return; // 此时必然正在关闭；已接收任务全部取完。
            task = std::move(pending_.front());
            pending_.pop();
        }
        // 不持有队列锁执行计算；packaged_task 将分析异常传给 future.get()。
        task();
    }
}

void Executor::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    ready_.notify_all();
    // [05] exchange 转移 join 责任，并将成员容器置为空，支持顺序重复关闭。
    auto joining = std::exchange(workers_, std::vector<std::thread>{});
    for (auto& worker : joining) {
        if (worker.joinable()) worker.join();
    }
}

} // namespace signal_lab
