#pragma once

#include <cstdint>
#include <cstring>
#include <simlib/errmsg.hh>
#include <simlib/logger.hh>
#include <simlib/macros/throw.hh>
#include <simlib/string_transform.hh>
#include <string_view>
#include <sys/utsname.h>

struct KernelVersion {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
};

constexpr bool operator==(const KernelVersion& a, const KernelVersion& b) noexcept {
    return a.major == b.major && a.minor == b.minor && a.patch == b.patch;
}

constexpr bool operator!=(const KernelVersion& a, const KernelVersion& b) noexcept {
    return !(a == b);
}

constexpr bool operator<(const KernelVersion& a, const KernelVersion& b) noexcept {
    if (a.major != b.major) {
        return a.major < b.major;
    }
    if (a.minor != b.minor) {
        return a.minor < b.minor;
    }
    return a.patch < b.patch;
}

constexpr bool operator>(const KernelVersion& a, const KernelVersion& b) noexcept { return b < a; }

constexpr bool operator<=(const KernelVersion& a, const KernelVersion& b) noexcept {
    return !(b < a);
}

constexpr bool operator>=(const KernelVersion& a, const KernelVersion& b) noexcept {
    return !(a < b);
}

inline KernelVersion kernel_version() {
    utsname un;
    if (uname(&un)) {
        THROW("uname()", errmsg());
    }
    auto s = un.release;
    size_t first_dot_pos = 0;
    while (s[first_dot_pos] != '.') {
        if (s[first_dot_pos] == '\0') {
            THROW("wrong release returned from uname()");
        }
        ++first_dot_pos;
    }
    size_t second_dot_pos = first_dot_pos + 1;
    while (s[second_dot_pos] != '.') {
        if (s[second_dot_pos] == '\0') {
            THROW("wrong release returned from uname()");
        }
        ++second_dot_pos;
    }
    size_t end = second_dot_pos + 1;
    while ('0' <= s[end] && s[end] <= '9') {
        ++end;
    }

    auto major = str2num<decltype(KernelVersion::major)>(std::string_view{s, first_dot_pos});
    auto minor = str2num<decltype(KernelVersion::minor)>(
        std::string_view{s + first_dot_pos + 1, second_dot_pos - first_dot_pos - 1}
    );
    auto patch = str2num<decltype(KernelVersion::patch)>(std::string_view{
        s + second_dot_pos + 1,
        end - second_dot_pos - 1,
    });
    if (!major || !minor || !patch) {
        THROW("wrong release returned from uname()");
    }
    return {
        .major = *major,
        .minor = *minor,
        .patch = *patch,
    };
}
