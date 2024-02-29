#include "assert_result.hh"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <simlib/file_descriptor.hh>
#include <simlib/sandbox/sandbox.hh>

// NOLINTNEXTLINE
TEST(sandbox, exit_code_is_correct) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(sc.await_result(sc.send_request({{"/bin/true"}})), CLD_EXITED, 0);
    ASSERT_RESULT_OK(sc.await_result(sc.send_request({{"/bin/false"}})), CLD_EXITED, 1);
}

// NOLINTNEXTLINE
TEST(sandbox, three_request_of_which_second_is_invalid) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(sc.await_result(sc.send_request({{"/bin/true"}})), CLD_EXITED, 0);
    ASSERT_RESULT_ERROR(
        sc.await_result(sc.send_request({{"."}})),
        "tracee: execveat() - Permission denied (os error 13)"
    );
    ASSERT_RESULT_OK(sc.await_result(sc.send_request({{"/bin/false"}})), CLD_EXITED, 1);
}

// NOLINTNEXTLINE
TEST(sandbox, empty_argv) {
    auto sc = sandbox::spawn_supervisor();
    auto rh = sc.send_request(FileDescriptor{"/bin/true", O_RDONLY | O_CLOEXEC}, {}, {});
    ASSERT_RESULT_OK(sc.await_result(std::move(rh)), CLD_EXITED, 0);
}

// NOLINTNEXTLINE
TEST(sandbox, awaiting_in_custom_order) {
    auto sc = sandbox::spawn_supervisor();
    auto rh1 = sc.send_request({{"/bin/true"}});
    auto rh2 = sc.send_request({{"/bin/false"}});
    ASSERT_RESULT_OK(sc.await_result(std::move(rh2)), CLD_EXITED, 1);
    ASSERT_RESULT_OK(sc.await_result(std::move(rh1)), CLD_EXITED, 0);
}
