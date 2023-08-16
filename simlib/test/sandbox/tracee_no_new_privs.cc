#include "../gtest_with_tester.hh"
#include "assert_result.hh"

#include <gtest/gtest.h>
#include <simlib/sandbox/sandbox.hh>
#include <unistd.h>

// NOLINTNEXTLINE
TEST(sandbox, trace_has_no_new_privs_attribute_set) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{tester_executable_path}}, {.stderr_fd = STDERR_FILENO});
    ASSERT_RESULT_OK(sc.await_result(), CLD_EXITED, 0);
}
