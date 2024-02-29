#include "../gtest_with_tester.hh"
#include "assert_result.hh"

#include <simlib/sandbox/sandbox.hh>

// NOLINTNEXTLINE
TEST(sandbox, stdin_is_RDONLY_dev_null_stdout_and_stderr_are_WRONLY_dev_null) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(sc.await_result(sc.send_request({{tester_executable_path}})), CLD_EXITED, 0);
}
