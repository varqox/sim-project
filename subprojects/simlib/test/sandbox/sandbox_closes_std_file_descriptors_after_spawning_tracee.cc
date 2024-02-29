#include "../gtest_with_tester.hh"
#include "assert_result.hh"

#include <gtest/gtest.h>
#include <optional>
#include <simlib/sandbox/sandbox.hh>
#include <sys/socket.h>
#include <unistd.h>

using std::nullopt;
using std::optional;

// NOLINTNEXTLINE
TEST(sandbox, test_closing_std_file_descriptors) {
    auto sc = sandbox::spawn_supervisor();
    auto test = [&](int in_fd, int out_fd, const char* tester_arg) {
        int sfds[2];
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sfds), 0);
        auto get_fd_for = [&](int fd) -> optional<int> {
            if (in_fd == fd) {
                return sfds[0];
            }
            if (out_fd == fd) {
                return sfds[1];
            }
            return nullopt;
        };
        auto rh = sc.send_request(
            {{tester_executable_path, tester_arg}},
            {
                .stdin_fd = get_fd_for(STDIN_FILENO),
                .stdout_fd = get_fd_for(STDOUT_FILENO),
                .stderr_fd = get_fd_for(STDERR_FILENO),
            }
        );
        ASSERT_EQ(close(sfds[0]), 0);
        ASSERT_EQ(close(sfds[1]), 0);
        // Now only tracee should own the file descriptors
        ASSERT_RESULT_OK(sc.await_result(std::move(rh)), CLD_EXITED, 0);
    };
    test(STDIN_FILENO, STDOUT_FILENO, "io");
    test(STDOUT_FILENO, STDERR_FILENO, "oe");
    test(STDERR_FILENO, STDIN_FILENO, "ei");
}
