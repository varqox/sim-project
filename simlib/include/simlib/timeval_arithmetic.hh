#pragma once

#include <sys/time.h>

constexpr timeval operator+(timeval a, timeval b) noexcept {
    timeval res{a.tv_sec + b.tv_sec, a.tv_usec + b.tv_usec};
    if (res.tv_usec >= 1'000'000) {
        ++res.tv_sec;
        res.tv_usec -= 1'000'000;
    }

    return res;
}

constexpr timeval operator-(timeval a, timeval b) noexcept {
    timeval res{a.tv_sec - b.tv_sec, a.tv_usec - b.tv_usec};
    if (res.tv_usec < 0) {
        --res.tv_sec;
        res.tv_usec += 1'000'000;
    }

    return res;
}

constexpr timeval& operator+=(timeval& a, timeval b) noexcept { return (a = a + b); }

constexpr timeval& operator-=(timeval& a, timeval b) noexcept { return (a = a - b); }

constexpr bool operator==(timeval a, timeval b) noexcept {
    return (a.tv_sec == b.tv_sec and a.tv_usec == b.tv_usec);
}

constexpr bool operator!=(timeval a, timeval b) noexcept {
    return (a.tv_sec != b.tv_sec or a.tv_usec != b.tv_usec);
}

constexpr bool operator<(timeval a, timeval b) noexcept {
    return (a.tv_sec < b.tv_sec or (a.tv_sec == b.tv_sec and a.tv_usec < b.tv_usec));
}

constexpr bool operator>(timeval a, timeval b) noexcept { return b < a; }

constexpr bool operator<=(timeval a, timeval b) noexcept {
    return (a.tv_sec < b.tv_sec or (a.tv_sec == b.tv_sec and a.tv_usec <= b.tv_usec));
}

constexpr bool operator>=(timeval a, timeval b) noexcept { return b <= a; }
