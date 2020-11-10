#pragma once

#include <csignal>
#include <utility>

// Block all signals
template <int (*func)(int, const sigset_t*, sigset_t*)>
class SignalBlockerBase {
private:
    sigset_t old_mask{};

public:
    const static sigset_t empty_mask, full_mask;

    SignalBlockerBase() noexcept { block(); }

    SignalBlockerBase(const SignalBlockerBase&) = delete;
    SignalBlockerBase(SignalBlockerBase&&) = delete;
    SignalBlockerBase& operator=(const SignalBlockerBase&) = delete;
    SignalBlockerBase& operator=(SignalBlockerBase&&) = delete;

    [[nodiscard]] int block() noexcept { return func(SIG_SETMASK, &full_mask, &old_mask); }

    [[nodiscard]] int unblock() noexcept { return func(SIG_SETMASK, &old_mask, nullptr); }

    ~SignalBlockerBase() noexcept { unblock(); }

private:
    static sigset_t empty_mask_val() noexcept {
        sigset_t mask;
        sigemptyset(&mask);
        return mask;
    };

    static sigset_t full_mask_val() noexcept {
        sigset_t mask;
        sigfillset(&mask);
        return mask;
    };
};

template <int (*func)(int, const sigset_t*, sigset_t*)>
const sigset_t SignalBlockerBase<func>::empty_mask = SignalBlockerBase<func>::empty_mask_val();

template <int (*func)(int, const sigset_t*, sigset_t*)>
const sigset_t SignalBlockerBase<func>::full_mask = SignalBlockerBase<func>::full_mask_val();

using SignalBlocker = SignalBlockerBase<sigprocmask>;
using ThreadSignalBlocker = SignalBlockerBase<pthread_sigmask>;

// Block all signals when function @p f is called
template <class F, class... Args>
auto block_signals(F&& f, Args&&... args) -> decltype(f(std::forward<Args>(args)...)) {
    SignalBlocker sb;
    return f(std::forward<Args>(args)...);
}

#define BLOCK_SIGNALS(...) block_signals([&] { return __VA_ARGS__; })

// Block all signals when function @p f is called
template <class F, class... Args>
auto thread_block_signals(F&& f, Args&&... args) -> decltype(f(std::forward<Args>(args)...)) {
    ThreadSignalBlocker sb;
    return f(std::forward<Args>(args)...);
}

#define THREAD_BLOCK_SIGNALS(...) thread_block_signals([&] { return __VA_ARGS__; })
