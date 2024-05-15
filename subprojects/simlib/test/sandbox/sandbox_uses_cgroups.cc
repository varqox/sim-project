#include "../gtest_with_tester.hh"
#include "assert_result.hh"
#include "mount_operations_mount_proc_if_running_under_leak_sanitizer.hh"

#include <simlib/sandbox/sandbox.hh>
#include <unistd.h>

// NOLINTNEXTLINE
TEST(sandbox, test_each_of_pid1_and_tracee_gets_its_own_cgroup) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{tester_executable_path}},
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
        )),
        CLD_EXITED,
        0
    );
}
