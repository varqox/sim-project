#include "assert_result.hh"

#include <exception>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simlib/sandbox/sandbox.hh>

// NOLINTNEXTLINE
TEST(sandbox, nonexistent_argv0) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_THAT(
        [&] { sc.send_request({{""}}); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("open(\"\") - No such file or directory (os error 2)")
        )
    );
}

// NOLINTNEXTLINE
TEST(sandbox, empty_argv_without_fd) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_THAT(
        [&] { sc.send_request({}); },
        testing::ThrowsMessage<std::runtime_error>(testing::StartsWith("argv cannot be empty"))
    );
}

// NOLINTNEXTLINE
TEST(sandbox, nonexistent_executable_file_descriptor) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_THAT(
        [&] { sc.send_request(62356, {}, {}); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("sendmsg() - Bad file descriptor (os error 9)")
        )
    );
}

// NOLINTNEXTLINE
TEST(sandbox, negative_executable_file_descriptor) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_THAT(
        [&] { sc.send_request(-1, {}, {}); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("sendmsg() - Bad file descriptor (os error 9)")
        )
    );
}

// NOLINTNEXTLINE
TEST(sandbox, invalid_executable_file_descriptor) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{"."}});
    ASSERT_RESULT_ERROR(sc.await_result(), "tracee: execveat() - Permission denied (os error 13)");
}

// NOLINTNEXTLINE
TEST(sandbox, nonexistent_stdin_file_descriptor) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_THAT(
        [&] { sc.send_request({{"/bin/true"}}, {.stdin_fd = 62356}); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("sendmsg() - Bad file descriptor (os error 9)")
        )
    );
}

// NOLINTNEXTLINE
TEST(sandbox, negative_stdin_file_descriptor) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_THAT(
        [&] { sc.send_request({{"/bin/true"}}, {.stdin_fd = -1}); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("sendmsg() - Bad file descriptor (os error 9)")
        )
    );
}

// NOLINTNEXTLINE
TEST(sandbox, nonexistent_stdout_file_descriptor) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_THAT(
        [&] { sc.send_request({{"/bin/true"}}, {.stdout_fd = 62356}); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("sendmsg() - Bad file descriptor (os error 9)")
        )
    );
}

// NOLINTNEXTLINE
TEST(sandbox, negative_stdout_file_descriptor) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_THAT(
        [&] { sc.send_request({{"/bin/true"}}, {.stdout_fd = -1}); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("sendmsg() - Bad file descriptor (os error 9)")
        )
    );
}

// NOLINTNEXTLINE
TEST(sandbox, nonexistent_stderr_file_descriptor) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_THAT(
        [&] { sc.send_request({{"/bin/true"}}, {.stderr_fd = 62356}); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("sendmsg() - Bad file descriptor (os error 9)")
        )
    );
}

// NOLINTNEXTLINE
TEST(sandbox, negative_stderr_file_descriptor) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_THAT(
        [&] { sc.send_request({{"/bin/true"}}, {.stderr_fd = -1}); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("sendmsg() - Bad file descriptor (os error 9)")
        )
    );
}
