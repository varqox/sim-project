#include "../gtest_with_tester.hh"
#include "assert_result.hh"

#include <simlib/sandbox/sandbox.hh>
#include <unistd.h>

// NOLINTNEXTLINE
TEST(sandbox, tracee_hacks_limits_by_disabling_cgroup_controllers) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request(
        {{tester_executable_path}},
        {
            .stderr_fd = STDERR_FILENO,
            .cgroup =
                {
                    .process_num_limit = 1,
                    .memory_limit_in_bytes = 10 << 20,
                },
        }
    );
    ASSERT_RESULT_OK(sc.await_result(), CLD_EXITED, 0);
}

// NOLINTNEXTLINE
TEST(sandbox, prevented_tracee_hacking_limits_by_disabling_cgroup_controllers) {
    using MountTmpfs = sandbox::RequestOptions::LinuxNamespaces::Mount::MountTmpfs;
    auto sc = sandbox::spawn_supervisor();
    sc.send_request(
        {{tester_executable_path}},
        {
            .stderr_fd = STDERR_FILENO,
            .linux_namespaces =
                {
                    .mount =
                        {
                            .operations = {{
                                MountTmpfs{.path = "/sys/fs/cgroup", .inode_limit = 0},
                            }},
                        },
                },
            .cgroup =
                {
                    .process_num_limit = 1,
                    .memory_limit_in_bytes = 10 << 20,
                },
        }
    );
    ASSERT_RESULT_OK(sc.await_result(), CLD_EXITED, 1);
}
