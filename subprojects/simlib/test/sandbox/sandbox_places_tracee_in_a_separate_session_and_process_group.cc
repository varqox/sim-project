#include "../gtest_with_tester.hh"
#include "assert_result.hh"

#include <simlib/sandbox/sandbox.hh>
#include <unistd.h>

// NOLINTNEXTLINE
TEST(sandbox, sandbox_places_tracee_in_a_separate_session_and_process) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request({{tester_executable_path}}, {.stderr_fd = STDERR_FILENO})),
        CLD_EXITED,
        0
    );
}
