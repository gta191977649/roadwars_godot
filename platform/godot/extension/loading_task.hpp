#pragma once
#include <coroutine>
#include <exception>
#include <string>
#include <utility>

namespace roadwars {
// Cooperative main-thread preparation; yields only after a completed resource unit.
struct LoadingProgress {
    float fraction{};
    std::string message;
};
class LoadingTask {
  public:
    struct promise_type {
        LoadingProgress progress;
        std::exception_ptr error;
        LoadingTask get_return_object() {
            return LoadingTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(LoadingProgress value) {
            progress = std::move(value);
            return {};
        }
        void return_void() noexcept {}
        void unhandled_exception() noexcept { error = std::current_exception(); }
    };
    LoadingTask() = default;
    ~LoadingTask() { reset(); }
    LoadingTask(LoadingTask&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    LoadingTask& operator=(LoadingTask&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    LoadingTask(const LoadingTask&) = delete;
    LoadingTask& operator=(const LoadingTask&) = delete;
    explicit operator bool() const { return static_cast<bool>(handle_); }
    bool advance() {
        if (!handle_ || handle_.done())
            return false;
        handle_.resume();
        if (handle_.promise().error)
            std::rethrow_exception(handle_.promise().error);
        return !handle_.done();
    }
    const LoadingProgress& progress() const { return handle_.promise().progress; }
    void reset() {
        if (handle_) {
            handle_.destroy();
            handle_ = {};
        }
    }

  private:
    explicit LoadingTask(std::coroutine_handle<promise_type> handle) : handle_(handle) {}
    std::coroutine_handle<promise_type> handle_{};
};
} // namespace roadwars
