#include "../gtest_with_tester.hh"
#include "assert_result.hh"
#include "mount_operations_mount_proc_if_running_under_leak_sanitizer.hh"

#include <simlib/sandbox/sandbox.hh>
#include <string_view>
#include <unistd.h>

// NOLINTNEXTLINE
TEST(sandbox, sandbox_uses_net_namespace) {
    std::array<char, 32> time_ns_id;
    auto time_ns_id_len = readlink("/proc/self/ns/time", time_ns_id.data(), time_ns_id.size());
    ASSERT_GT(time_ns_id_len, 0);
    ASSERT_LT(time_ns_id_len, time_ns_id.size());

    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{tester_executable_path, std::string_view(time_ns_id.data(), time_ns_id_len)}},
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
