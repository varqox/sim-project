#pragma once

#include <chrono>
#include <poll.h>
#include <simlib/file_contents.hh>
#include <simlib/macros/throw.hh>
#include <simlib/sim/judge/language_suite/suite.hh>
#include <simlib/temporary_file.hh>

inline void
compile_for_fresh_execution(sim::judge::language_suite::Suite& suite, StringView source) {
    auto file = TemporaryFile{"/tmp/sim_fresh_execution_test.XXXXXX"};
    put_file_contents(file.path(), source);
    auto result = suite.compile(
        file.path(),
        {
            .time_limit = std::chrono::seconds{60},
            .cpu_time_limit = std::chrono::seconds{20},
            .memory_limit_in_bytes = 1 << 30,
            .max_file_size_in_bytes = 20 << 20,
        }
    );
    if (result.is_err()) {
        THROW("Compilation failed: ", get_file_contents(std::move(result).unwrap_err(), 0, -1));
    }
}

inline auto fresh_execution_options() {
    return sim::judge::language_suite::Suite::RunOptions{
        .stdin_fd = std::nullopt,
        .stdout_fd = std::nullopt,
        .stderr_fd = std::nullopt,
        .time_limit = std::chrono::seconds{20},
        .cpu_time_limit = std::chrono::seconds{1},
        .memory_limit_in_bytes = 64 << 20,
        .max_stack_size_in_bytes = 64 << 20,
        .max_file_size_in_bytes = 0,
    };
}

inline bool readable_within(int fd, std::chrono::milliseconds timeout) {
    auto pfd = pollfd{.fd = fd, .events = POLLIN, .revents = 0};
    return poll(&pfd, 1, timeout.count()) == 1 && (pfd.revents & POLLIN);
}
