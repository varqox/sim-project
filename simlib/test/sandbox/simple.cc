#include <gtest/gtest.h>
#include <simlib/sandbox/sandbox.hh>

// NOLINTNEXTLINE
TEST(sandbox, exit_code_is_correct) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request("true");
    ASSERT_EQ(std::get<sandbox::ResultOk>(sc.await_result()).system_result, 0);
    sc.send_request("false");
    ASSERT_EQ(std::get<sandbox::ResultOk>(sc.await_result()).system_result, 1 << 8);
}
