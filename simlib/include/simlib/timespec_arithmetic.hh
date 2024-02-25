#pragma once

#include <sys/time.h>

constexpr timespec operator+(timespec a, timespec b) noexcept {
    timespec res{a.tv_sec + b.tv_sec, a.tv_nsec + b.tv_nsec};
    if (res.tv_nsec >= 1'000'000'000) {
        ++res.tv_sec;
        res.tv_nsec -= 1'000'000'000;
    }

    return res;
}

constexpr timespec operator-(timespec a, timespec b) noexcept {
    timespec res{a.tv_sec - b.tv_sec, a.tv_nsec - b.tv_nsec};
    if (res.tv_nsec < 0) {
        --res.tv_sec;
        res.tv_nsec += 1'000'000'000;
    }

    return res;
}

constexpr timespec& operator+=(timespec& a, timespec b) noexcept { return (a = a + b); }

constexpr timespec& operator-=(timespec& a, timespec b) noexcept { return (a = a - b); }

constexpr bool operator==(timespec a, timespec b) noexcept {
    return (a.tv_sec == b.tv_sec and a.tv_nsec == b.tv_nsec);
}

constexpr bool operator!=(timespec a, timespec b) noexcept {
    return (a.tv_sec != b.tv_sec or a.tv_nsec != b.tv_nsec);
}

constexpr bool operator<(timespec a, timespec b) noexcept {
    return (a.tv_sec < b.tv_sec or (a.tv_sec == b.tv_sec and a.tv_nsec < b.tv_nsec));
}

constexpr bool operator>(timespec a, timespec b) noexcept { return b < a; }

constexpr bool operator<=(timespec a, timespec b) noexcept {
    return (a.tv_sec < b.tv_sec or (a.tv_sec == b.tv_sec and a.tv_nsec <= b.tv_nsec));
}

constexpr bool operator>=(timespec a, timespec b) noexcept { return b <= a; }
