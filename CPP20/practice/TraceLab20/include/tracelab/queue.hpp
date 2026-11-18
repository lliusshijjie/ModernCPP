#pragma once
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>
#include <stdexcept>

namespace tracelab {
template <class T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {
        if (!capacity) {
            throw std::invalid_argument("zero queue capacity");
        }
    }

    bool push(T item, std::stop_token stop) {
        std::unique_lock lock(mutex_);
        changed_.wait(lock, stop, [&] { return closed_ || queue_.size() < capacity_; });
        if (closed_ || stop.stop_requested()) {
            return false;
        }
        queue_.push_back(std::move(item));
        lock.unlock();
        changed_.notify_all();
        return true;
    }

    std::optional<T> pop(std::stop_token stop) {
        std::unique_lock lock(mutex_);
        changed_.wait(lock, stop, [&] { return closed_ || !queue_.empty(); });
        if (stop.stop_requested() || queue_.empty()) {
            return {};
        }
        auto item = std::move(queue_.front());
        queue_.pop_front();
        lock.unlock();
        changed_.notify_all();
        return item;
    }

    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        changed_.notify_all();
    }

private:
    const std::size_t capacity_;
    std::mutex mutex_;
    std::condition_variable_any changed_;
    std::deque<T> queue_;
    bool closed_{};
};
} // namespace tracelab
