#include <gtest/gtest.h>
#include <netinet/in.h>
#include <simlib/concat_tostr.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/string_view.hh>
#include <simlib/throw_assert.hh>
#include <string>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using std::string;
using std::vector;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static CStringView supervisor_executable_path;

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    throw_assert(argc == 2);
    supervisor_executable_path = CStringView{argv[1]};
    return RUN_ALL_TESTS();
}

struct Socket {
    FileDescriptor our_end;
    FileDescriptor supervisor_end;
};

Socket open_socket(int domain, int type, int protocol = 0) {
    int sock_fds[2];
    throw_assert(socketpair(domain, type, protocol, sock_fds) == 0);
    return {
        .our_end = FileDescriptor{sock_fds[0]},
        .supervisor_end = FileDescriptor{sock_fds[1]},
    };
}

struct RunResult {
    string output;
    bool exited0;
};

RunResult run_supervisor(vector<string> args, Socket* sock_fd) {
    // NOLINTNEXTLINE(android-cloexec-memfd-create)
    FileDescriptor output_fd{memfd_create("sandbox supervisor output", MFD_CLOEXEC)};
    throw_assert(output_fd.is_open());
    pid_t pid = fork();
    throw_assert(pid != -1);
    if (pid == 0) {
        if (sock_fd) {
            throw_assert(sock_fd->our_end.close() == 0);
        }
        vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (auto& arg : args) {
            argv.emplace_back(arg.data());
        }
        argv.emplace_back(nullptr);
        throw_assert(dup3(output_fd, STDERR_FILENO, 0) == STDERR_FILENO);
        execve(supervisor_executable_path.c_str(), argv.data(), environ);
        _exit(1);
    }
    if (sock_fd) {
        throw_assert(sock_fd->supervisor_end.close() == 0);
        // Make supervisor exit immediately because of no awaiting requests
        throw_assert(sock_fd->our_end.close() == 0);
    }
    int status = 0;
    throw_assert(waitpid(pid, &status, 0) == pid);
    return {
        .output = get_file_contents(output_fd, 0, -1),
        .exited0 = WIFEXITED(status) && WEXITSTATUS(status) == 0,
    };
}

// NOLINTNEXTLINE
TEST(sandbox, sandbox_supervisor_no_arguments) {
    auto res = run_supervisor({"supervisor"}, nullptr);
    ASSERT_EQ(res.output, "Usage: supervisor <unix socket file descriptor number>");
    ASSERT_FALSE(res.exited0);
}

// NOLINTNEXTLINE
TEST(sandbox, sandbox_supervisor_two_arguments) {
    auto res = run_supervisor({"supervisor", "1", "2"}, nullptr);
    ASSERT_EQ(res.output, "Usage: supervisor <unix socket file descriptor number>");
    ASSERT_FALSE(res.exited0);
}

// NOLINTNEXTLINE
TEST(sandbox, sandbox_supervisor_non_number_argument) {
    auto res = run_supervisor({"supervisor", "x"}, nullptr);
    ASSERT_EQ(res.output, "supervisor: invalid file descriptor number as argument");
    ASSERT_FALSE(res.exited0);
}

// NOLINTNEXTLINE
TEST(sandbox, sandbox_supervisor_invalid_fd_number) {
    auto res = run_supervisor({"supervisor", "642"}, nullptr);
    ASSERT_EQ(
        res.output,
        "supervisor: invalid file descriptor (getsockopt() - Bad file descriptor (os error 9))"
    );
    ASSERT_FALSE(res.exited0);
}

// NOLINTNEXTLINE
TEST(sandbox, sandbox_supervisor_fd_is_not_a_socket) {
    auto res = run_supervisor({"supervisor", "0"}, nullptr);
    ASSERT_EQ(
        res.output,
        "supervisor: invalid file descriptor (getsockopt() - Socket operation on non-socket (os "
        "error 88))"
    );
    ASSERT_FALSE(res.exited0);
}

// // NOLINTNEXTLINE
TEST(sandbox, sandbox_supervisor_fd_is_socket_of_invalid_domain) {
    auto sock_fd = FileDescriptor{socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)};
    throw_assert(sock_fd.is_open());
    auto res = run_supervisor({"supervisor", concat_tostr(static_cast<int>(sock_fd))}, nullptr);
    ASSERT_EQ(res.output, "supervisor: invalid socket domain, expected AF_UNIX");
    ASSERT_FALSE(res.exited0);
}

// NOLINTNEXTLINE
TEST(sandbox, sandbox_supervisor_fd_is_socket_of_invalid_type) {
    auto socket = open_socket(AF_UNIX, SOCK_DGRAM);
    auto res = run_supervisor(
        {"supervisor", concat_tostr(static_cast<int>(socket.supervisor_end))}, &socket
    );
    ASSERT_EQ(res.output, "supervisor: invalid socket type, expected SOCK_STREAM");
    ASSERT_FALSE(res.exited0);
}

// NOLINTNEXTLINE
TEST(sandbox, sandbox_supervisor_fd_is_correct) {
    auto socket = open_socket(AF_UNIX, SOCK_STREAM);
    auto res = run_supervisor(
        {"supervisor", concat_tostr(static_cast<int>(socket.supervisor_end))}, &socket
    );
    ASSERT_TRUE(res.exited0);
    ASSERT_EQ(res.output, "");
}
