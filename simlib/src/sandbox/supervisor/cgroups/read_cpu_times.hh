#pragma once

#include <array>
#include <cerrno>
#include <cstdint>
#include <exception>
#include <optional>
#include <simlib/errmsg.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_transform.hh>
#include <simlib/string_view.hh>
#include <sys/types.h>
#include <unistd.h>

namespace sandbox::supervisor::cgroups {

struct CpuTimes {
    uint64_t user_usec;
    uint64_t system_usec;
};

template <class ErrorHandler>
[[nodiscard]] CpuTimes
read_cpu_times(int cgroup_cpu_stat_fd, ErrorHandler no_return_error_handler) noexcept {
    std::array<
        char,
        sizeof("usage_usec 18446744073709551615\n"
               "user_usec 18446744073709551615\n"
               "system_usec 18446744073709551615\n"
        ) - 1>
        buff;
    ssize_t len;
    do {
        len = pread(cgroup_cpu_stat_fd, buff.data(), buff.size(), 0);
    } while (len < 0 && errno == EINTR);
    if (len < 0) {
        no_return_error_handler("pread()", errmsg());
        std::terminate();
    }

    std::optional<decltype(CpuTimes::user_usec)> user_usec;
    std::optional<decltype(CpuTimes::system_usec)> system_usec;
    StringView data{buff.data(), static_cast<size_t>(len)};
    for (;;) {
        auto newline_pos = data.find('\n');
        if (newline_pos == StringView::npos) {
            break; // ignore partially read line
        }
        auto line = data.extract_prefix(newline_pos);
        data.remove_prefix(1); // remove newline
        if (has_prefix(line, "user_usec ")) {
            user_usec =
                str2num<decltype(CpuTimes::user_usec)>(line.without_prefix(std::strlen("user_usec ")
                ));
            if (!user_usec) {
                no_return_error_handler("parsing failed: line: ", line);
                std::terminate();
            }
        } else if (has_prefix(line, "system_usec ")) {
            system_usec = str2num<decltype(CpuTimes::system_usec)>(
                line.without_prefix(std::strlen("system_usec "))
            );
            if (!system_usec) {
                no_return_error_handler("parsing failed: line: ", line);
                std::terminate();
            }
        }
    }
    if (!user_usec) {
        no_return_error_handler("parsing failed: missing user_usec");
        std::terminate();
    }
    if (!system_usec) {
        no_return_error_handler("parsing failed: missing system_usec");
        std::terminate();
    }

    return {
        .user_usec = *user_usec,
        .system_usec = *system_usec,
    };
}

} // namespace sandbox::supervisor::cgroups
