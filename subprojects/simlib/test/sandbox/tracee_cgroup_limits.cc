#include "../gtest_with_tester.hh"
#include "assert_result.hh"
#include "simlib/leak_sanitizer.hh"

#include <chrono>
#include <simlib/address_sanitizer.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/time_format_conversions.hh>
#include <unistd.h>

using sandbox::result::Ok;

// NOLINTNEXTLINE
TEST(sandbox, no_limit) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request({{tester_executable_path}}, {.stderr_fd = STDERR_FILENO})),
        CLD_EXITED,
        0
    );
}

// NOLINTNEXTLINE
TEST(sandbox, process_num_limit) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_ERROR(
        sc.await_result(sc.send_request({{"/bin/true"}}, {.cgroup = {.process_num_limit = 0}})),
        "pid1: clone3() - Resource temporarily unavailable (os error 11)"
    );
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request({{"/bin/true"}}, {.cgroup = {.process_num_limit = 1}})),
        CLD_EXITED,
        0
    );

    // AddressSanitizer uses threads internally making tester fail with such low limit
        ASSERT_RESULT_OK(
            sc.await_result(sc.send_request(
                {{tester_executable_path, "pids_limit"}},
                {
                    .stderr_fd = STDERR_FILENO,
                    .cgroup = {.process_num_limit = 1 + LEAK_SANITIZER},
                }
            )),
            CLD_EXITED,
            0
        );
}

// NOLINTNEXTLINE
TEST(sandbox, memory_limit) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_ERROR(
        sc.await_result(sc.send_request(
            {{"/bin/true"}},
            {.cgroup =
                 {
                     .memory_limit_in_bytes = 0,
                     .swap_limit_in_bytes = 0,
                 }}
        )),
        "tracee process died unexpectedly before execveat() without an error message: killed by "
        "signal KILL - Killed"
    );
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{"/bin/true"}},
            {.cgroup =
                 {
                     .memory_limit_in_bytes = 2 << 20,
                     .swap_limit_in_bytes = 0,
                 }}
        )),
        CLD_EXITED,
        0
    );
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{tester_executable_path, "check_memory_limit"}},
            {
                .stderr_fd = STDERR_FILENO,
                .cgroup = {.memory_limit_in_bytes = 2 << 20, .swap_limit_in_bytes = 0},
            }
        )),
        CLD_KILLED,
        SIGKILL
    );
}

// NOLINTNEXTLINE
TEST(sandbox, process_num_and_memory_limit) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{tester_executable_path, "pids_limit", "check_memory_limit"}},
            {
                .stderr_fd = STDERR_FILENO,
                .cgroup =
                    {
                        .process_num_limit = 1 + LEAK_SANITIZER,
                        .memory_limit_in_bytes = 2 << 20,
                        .swap_limit_in_bytes = 0,
                    },
            }
        )),
        CLD_KILLED,
        SIGKILL
    );
}

// NOLINTNEXTLINE
TEST(sandbox, cpu_max_bandwidth) {
    auto sc = sandbox::spawn_supervisor();
    auto res_var = sc.await_result(sc.send_request(
        {{tester_executable_path, "cpu_max_bandwidth"}},
        {
            .stderr_fd = STDERR_FILENO,
            .cgroup =
                {
                    .cpu_max_bandwidth =
                        sandbox::RequestOptions::Cgroup::CpuMaxBandwidth{
                            .max_usec = 1000,
                            .period_usec = 2000,
                        },
                },
            .cpu_time_limit = std::chrono::milliseconds{10},
        }
    ));
    ASSERT_RESULT_OK(res_var, CLD_KILLED, SIGKILL);
    auto res = std::get<Ok>(res_var);
    ASSERT_GE(res.runtime, std::chrono::microseconds{17499})
        << "real: " << to_string(res.runtime).c_str()
        << "\ncpu:  " << to_string(res.cgroup.cpu_time.total()).c_str();
}
