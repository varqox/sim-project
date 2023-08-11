#include "../gtest_with_tester.hh"
#include "assert_result.hh"

#include <simlib/sandbox/sandbox.hh>
#include <unistd.h>

using sandbox::result::Ok;

// NOLINTNEXTLINE
TEST(sandbox, tracee_memory_only_mmapped) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{tester_executable_path, "17"}}, {.stderr_fd = STDERR_FILENO});
    auto res = sc.await_result();
    ASSERT_RESULT_OK(res, CLD_EXITED, 0);
    ASSERT_LT(std::get<Ok>(res).cgroup.peak_memory_in_bytes, 4 << 20);
}

// NOLINTNEXTLINE
TEST(sandbox, tracee_memory_initialized) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{tester_executable_path, "17", "fill"}}, {.stderr_fd = STDERR_FILENO});
    auto res = sc.await_result();
    ASSERT_RESULT_OK(res, CLD_EXITED, 0);
    ASSERT_GE(std::get<Ok>(res).cgroup.peak_memory_in_bytes, 17 << 20);
}
