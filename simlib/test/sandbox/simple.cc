#include "assert_result.hh"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <simlib/file_descriptor.hh>
#include <simlib/sandbox/sandbox.hh>

// NOLINTNEXTLINE
TEST(sandbox, exit_code_is_correct) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{"/bin/true"}});
    ASSERT_RESULT_OK(sc.await_result(), CLD_EXITED, 0);

    sc.send_request({{"/bin/false"}});
    ASSERT_RESULT_OK(sc.await_result(), CLD_EXITED, 1);
}

// NOLINTNEXTLINE
TEST(sandbox, three_request_of_which_second_is_invalid) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{"/bin/true"}});
    ASSERT_RESULT_OK(sc.await_result(), CLD_EXITED, 0);

    sc.send_request({{"."}});
    ASSERT_RESULT_ERROR(sc.await_result(), "tracee: execveat() - Permission denied (os error 13)");

    sc.send_request({{"/bin/false"}});
    ASSERT_RESULT_OK(sc.await_result(), CLD_EXITED, 1);
}

// NOLINTNEXTLINE
TEST(sandbox, empty_argv) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request(FileDescriptor{"/bin/true", O_RDONLY | O_CLOEXEC}, {}, {});
    ASSERT_RESULT_OK(sc.await_result(), CLD_EXITED, 0);
}
