#include "../gtest_with_tester.hh"
#include "assert_result.hh"

#include <chrono>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/time_format_conversions.hh>
#include <unistd.h>

using sandbox::result::Ok;

// NOLINTNEXTLINE
TEST(sandbox, no_limit) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{tester_executable_path}}, {.stderr_fd = STDERR_FILENO});
    ASSERT_RESULT_OK(sc.await_result(), CLD_EXITED, 0);
}

// NOLINTNEXTLINE
TEST(sandbox, process_num_limit) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{"/bin/true"}}, {.cgroup = {.process_num_limit = 0}});
    ASSERT_RESULT_ERROR(
        sc.await_result(), "pid1: clone3() - Resource temporarily unavailable (os error 11)"
    );

    sc.send_request({{"/bin/true"}}, {.cgroup = {.process_num_limit = 1}});
    ASSERT_RESULT_OK(sc.await_result(), CLD_EXITED, 0);

    sc.send_request(
        {{tester_executable_path, "pids_limit"}},
        {
            .stderr_fd = STDERR_FILENO,
            .cgroup = {.process_num_limit = 1},
        }
    );
    ASSERT_RESULT_OK(sc.await_result(), CLD_EXITED, 0);
}

// NOLINTNEXTLINE
TEST(sandbox, memory_limit) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{"/bin/true"}}, {.cgroup = {.memory_limit_in_bytes = 0}});
    ASSERT_RESULT_ERROR(
        sc.await_result(),
        "tracee process died unexpectedly before execveat() without an error message: killed by "
        "signal KILL - Killed"
    );

    sc.send_request({{"/bin/true"}}, {.cgroup = {.memory_limit_in_bytes = 2 << 20}});
    ASSERT_RESULT_OK(sc.await_result(), CLD_EXITED, 0);

    sc.send_request(
        {{tester_executable_path, "check_memory_limit"}},
        {
            .stderr_fd = STDERR_FILENO,
            .cgroup = {.memory_limit_in_bytes = 2 << 20},
        }
    );
    ASSERT_RESULT_OK(sc.await_result(), CLD_KILLED, SIGKILL);
}

// NOLINTNEXTLINE
TEST(sandbox, process_num_and_memory_limit) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request(
        {{tester_executable_path, "pids_limit", "check_memory_limit"}},
        {
            .stderr_fd = STDERR_FILENO,
            .cgroup = {.process_num_limit = 1, .memory_limit_in_bytes = 2 << 20},
        }
    );
    ASSERT_RESULT_OK(sc.await_result(), CLD_KILLED, SIGKILL);
}

// NOLINTNEXTLINE
TEST(sandbox, cpu_max_bandwidth) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request(
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
    );
    auto res_var = sc.await_result();
    ASSERT_RESULT_OK(res_var, CLD_KILLED, SIGKILL);
    auto res = std::get<Ok>(res_var);
    ASSERT_GE(res.runtime, std::chrono::microseconds{17499})
        << "real: " << to_string(res.runtime).c_str()
        << "\ncpu:  " << to_string(res.cgroup.cpu_time.total()).c_str();
}
