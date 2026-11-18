#pragma once
#include <coroutine>
#include <exception>
#include <iterator>
#include <memory>
#include <utility>

namespace tracelab {
// [39] The frame owns the yielded lvalue until the next resume.
template <class T>
class Generator {
public:
    struct promise_type;
    using Handle = std::coroutine_handle<promise_type>;

    struct promise_type {
        T* value{};
        std::exception_ptr error;

        Generator get_return_object() {
            return Generator(Handle::from_promise(*this));
        }

        std::suspend_always initial_suspend() noexcept {
            return {};
        }

        std::suspend_always final_suspend() noexcept {
            return {};
        }

        std::suspend_always yield_value(T& current) noexcept {
            value = std::addressof(current);
            return {};
        }

        void return_void() noexcept {
        }

        void unhandled_exception() noexcept {
            error = std::current_exception();
        }
    };

    struct iterator {
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using iterator_concept = std::input_iterator_tag;
        Handle handle{};

        T& operator*() const {
            return *handle.promise().value;
        }

        iterator& operator++() {
            handle.resume();
            if (handle.promise().error) {
                std::rethrow_exception(handle.promise().error);
            }
            return *this;
        }

        void operator++(int) {
            ++*this;
        }

        bool operator==(std::default_sentinel_t) const {
            return !handle || handle.done();
        }
    };

    Generator(Generator&& other) noexcept
        : handle_(std::exchange(other.handle_, {})), started_(other.started_) {
    }

    ~Generator() {
        if (handle_) {
            handle_.destroy();
        }
    }

    iterator begin() {
        iterator it{handle_};
        if (handle_ && !std::exchange(started_, true)) {
            ++it;
        }
        return it;
    }

    std::default_sentinel_t end() const {
        return {};
    }

private:
    explicit Generator(Handle handle) : handle_(handle) {
    }

    Handle handle_;
    bool started_{};
};
} // namespace tracelab
