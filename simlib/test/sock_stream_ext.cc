#include <cerrno>
#include <cstdint>
#include <gtest/gtest.h>
#include <simlib/file_descriptor.hh>
#include <simlib/socket_stream_ext.hh>
#include <simlib/throw_assert.hh>
#include <sys/socket.h>

using std::string;

struct Socket {
    FileDescriptor our_end;
    FileDescriptor other_end;
};

Socket open_socket(int domain, int type, int protocol = 0) {
    int sock_fds[2];
    throw_assert(socketpair(domain, type, protocol, sock_fds) == 0);
    return {
        .our_end = FileDescriptor{sock_fds[0]},
        .other_end = FileDescriptor{sock_fds[1]},
    };
}

// NOLINTNEXTLINE
TEST(socket_stream_ext, send_exact_0_bytes) { ASSERT_EQ(send_exact(-1, nullptr, 0, 0), 0); }

// NOLINTNEXTLINE
TEST(socket_stream_ext, recv_exact_0_bytes) { ASSERT_EQ(recv_exact(-1, nullptr, 0, 0), 0); }

// NOLINTNEXTLINE
TEST(socket_stream_ext, recv_exact_of_partial_writes) {
    auto socket = open_socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC);
    ASSERT_EQ(send_exact(socket.other_end, "abc", 3, 0), 0);
    pid_t pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        // This may be run with some delay, making the recv_exact() in the parent process read first
        // 3 bytes and needing to call the recv() again
        ASSERT_EQ(send_exact(socket.other_end, "xyz", 3, 0), 0);
        _exit(0);
    }
    string res = "123456";
    ASSERT_EQ(recv_exact(socket.our_end, res.data(), res.size(), 0), 0);
    ASSERT_EQ(res, "abcxyz");
    // Reap child
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// NOLINTNEXTLINE
TEST(socket_stream_ext, recv_exact_incomplete) {
    auto socket = open_socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC);
    ASSERT_EQ(send_exact(socket.other_end, "ab", 2, 0), 0);
    ASSERT_EQ(socket.other_end.close(), 0);
    char buff[3];
    ASSERT_EQ(recv_exact(socket.our_end, buff, 3, 0), -1);
    ASSERT_EQ(errno, EPIPE);
}

// NOLINTNEXTLINE
TEST(socket_stream_ext, send_exact_with_other_end_closed) {
    auto socket = open_socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC);
    ASSERT_EQ(socket.other_end.close(), 0);
    ASSERT_EQ(send_exact(socket.our_end, "a", 1, MSG_NOSIGNAL), -1);
    ASSERT_EQ(errno, EPIPE);
}

// NOLINTNEXTLINE
TEST(socket_stream_ext, invalid_file_descriptor) {
    auto socket = open_socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC);
    ASSERT_EQ(socket.our_end.close(), 0);
    ASSERT_EQ(send_exact(socket.our_end, "a", 1, MSG_NOSIGNAL), -1);
    ASSERT_EQ(errno, EBADF);
    char buff[1];
    ASSERT_EQ(recv_exact(socket.our_end, buff, 1, 0), -1);
    ASSERT_EQ(errno, EBADF);
}

// NOLINTNEXTLINE
TEST(socket_stream_ext, recv_as_partially_send) {
    auto socket = open_socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC);
    uint32_t x = 0xdeadbeef;
    ASSERT_EQ(send_exact(socket.other_end, &x, sizeof(x) / 2, 0), 0);
    pid_t pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        // This may be run with some delay, making the recv_as() in the parent process read first
        // 3 bytes and needing to call the recv() again
        ASSERT_EQ(
            send_exact(
                socket.other_end, reinterpret_cast<char*>(&x) + sizeof(x) / 2, sizeof(x) / 2, 0
            ),
            0
        );
        _exit(0);
    }
    uint32_t y;
    ASSERT_EQ(recv_bytes_as(socket.our_end, 0, y), 0);
    ASSERT_EQ(x, y);
    // Reap child
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// NOLINTNEXTLINE
TEST(socket_stream_ext, send_as_bytes) {
    auto socket = open_socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC);
    ASSERT_EQ(
        send_as_bytes(
            socket.other_end, 0, static_cast<uint8_t>(0x42), static_cast<uint32_t>(0xdeadbeef)
        ),
        0
    );
    ASSERT_EQ(socket.other_end.close(), 0);
    uint8_t x;
    uint32_t y;
    std::byte buff[sizeof(x) + sizeof(y)];
    ASSERT_EQ(recv_exact(socket.our_end, buff, sizeof(buff), 0), 0);
    std::memcpy(&x, buff, sizeof(x));
    std::memcpy(&y, buff + sizeof(x), sizeof(y));
    ASSERT_EQ(x, 0x42);
    ASSERT_EQ(y, 0xdeadbeef);
    ASSERT_EQ(recv(socket.our_end, buff, 1, 0), 0); // no extra bytes were sent
}

// NOLINTNEXTLINE
TEST(socket_stream_ext, recv_as_bytes) {
    auto socket = open_socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC);
    uint8_t x = 0x42;
    uint32_t y = 0xdeadbeef;
    std::byte buff[sizeof(x) + sizeof(y)];
    std::memcpy(buff, &x, sizeof(x));
    std::memcpy(buff + sizeof(x), &y, sizeof(y));
    ASSERT_EQ(send_exact(socket.other_end, buff, sizeof(buff), 0), 0);
    x = 0;
    y = 0;
    ASSERT_EQ(recv_bytes_as(socket.our_end, 0, x, y), 0);
    ASSERT_EQ(x, 0x42);
    ASSERT_EQ(y, 0xdeadbeef);
}
