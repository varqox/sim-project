#include "../gtest_with_tester.hh"
#include "assert_result.hh"

#include <simlib/sandbox/sandbox.hh>
#include <unistd.h>

// NOLINTNEXTLINE
TEST(sandbox, no_prlimit_limit) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{tester_executable_path}}, {.stderr_fd = STDERR_FILENO});
    ASSERT_RESULT_OK(sc.await_result(), CLD_EXITED, 0);
}

// NOLINTNEXTLINE
TEST(sandbox, prlimit_max_address_space_size_in_bytes) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request(
        {{tester_executable_path, "memory"}},
        {
            .stderr_fd = STDERR_FILENO,
            .prlimit =
                {
                    .max_address_space_size_in_bytes = 1 << 30,
                },
        }
    );
    ASSERT_RESULT_OK(sc.await_result(), CLD_EXITED, 0);
}
