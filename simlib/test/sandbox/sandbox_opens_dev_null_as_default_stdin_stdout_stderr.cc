#include "../gtest_with_tester.hh"

#include <simlib/sandbox/sandbox.hh>

// NOLINTNEXTLINE
TEST(sandbox, stdin_is_RDONLY_dev_null_stdout_and_stderr_are_WRONLY_dev_null) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request(tester_executable_path);
    ASSERT_EQ(std::get<sandbox::ResultOk>(sc.await_result()).system_result, 0);
}
