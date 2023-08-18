#include "../gtest_with_tester.hh"
#include "assert_result.hh"

#include <simlib/sandbox/sandbox.hh>
#include <unistd.h>

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
