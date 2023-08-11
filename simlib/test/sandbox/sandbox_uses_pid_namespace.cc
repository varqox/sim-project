#include "../gtest_with_tester.hh"
#include "assert_result.hh"

#include <simlib/sandbox/sandbox.hh>
#include <unistd.h>

// NOLINTNEXTLINE
TEST(sandbox, test_pids_and_kill_isolation) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{tester_executable_path}}, {.stderr_fd = STDERR_FILENO});
    ASSERT_RESULT_OK(sc.await_result(), CLD_EXITED, 0);
}
