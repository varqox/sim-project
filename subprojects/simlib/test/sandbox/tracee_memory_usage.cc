#include "../gtest_with_tester.hh"
#include "assert_result.hh"
#include "mount_operations_mount_proc_if_running_under_leak_sanitizer.hh"

#include <simlib/sandbox/sandbox.hh>
#include <unistd.h>

using sandbox::result::Ok;

// NOLINTNEXTLINE
TEST(sandbox, tracee_memory_only_mmapped) {
    auto sc = sandbox::spawn_supervisor();
    auto res = sc.await_result(sc.send_request(
        {{tester_executable_path, "33"}},
        {
            .stderr_fd = STDERR_FILENO,
            .linux_namespaces =
                {
                    .mount =
                        {
                            .operations =
                                mount_operations_mount_proc_if_running_under_leak_sanitizer(),
                        },
                },
        }
    ));
    ASSERT_RESULT_OK(res, CLD_EXITED, 0);
    ASSERT_LT(std::get<Ok>(res).cgroup.peak_memory_in_bytes, 32 << 20);
}

// NOLINTNEXTLINE
TEST(sandbox, tracee_memory_initialized) {
    auto sc = sandbox::spawn_supervisor();
    auto res = sc.await_result(sc.send_request(
        {{tester_executable_path, "33", "fill"}},
        {
            .stderr_fd = STDERR_FILENO,
            .linux_namespaces =
                {
                    .mount =
                        {
                            .operations =
                                mount_operations_mount_proc_if_running_under_leak_sanitizer(),
                        },
                },
        }
    ));
    ASSERT_RESULT_OK(res, CLD_EXITED, 0);
    ASSERT_GE(std::get<Ok>(res).cgroup.peak_memory_in_bytes, 33 << 20);
}
