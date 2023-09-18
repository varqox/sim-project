#pragma once

#include <cerrno>
#include <csignal>
#include <cstddef>
#include <pthread.h>
#include <simlib/throw_assert.hh>

class SignalCounter {
    int sig;
    size_t count = 0;
    sigset_t ss;
    sigset_t sigset_old;

public:
    explicit SignalCounter(int sig) : sig{sig} {
        throw_assert(sigemptyset(&ss) == 0);
        throw_assert(sigaddset(&ss, sig) == 0);
        throw_assert(pthread_sigmask(SIG_BLOCK, &ss, &sigset_old) == 0);
    }

    SignalCounter(const SignalCounter&) = delete;
    SignalCounter(SignalCounter&&) = delete;
    SignalCounter& operator=(const SignalCounter&) = delete;
    SignalCounter& operator=(SignalCounter&&) = delete;

    size_t get_count() {
        timespec ts = {.tv_sec = 0, .tv_nsec = 0};
        for (;;) {
            auto rc = sigtimedwait(&ss, nullptr, &ts);
            if (rc == sig) {
                ++count;
                continue;
            }
            throw_assert(rc == -1 && errno == EAGAIN);
            return count;
        }
    }

    ~SignalCounter() noexcept(false) {
        throw_assert(pthread_sigmask(SIG_SETMASK, &sigset_old, nullptr) == 0);
    }
};
