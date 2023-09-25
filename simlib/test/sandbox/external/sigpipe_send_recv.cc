#include <array>
#include <cerrno>
#include <csignal>
#include <ctime>
#include <gtest/gtest.h>
#include <simlib/throw_assert.hh>
#include <sys/socket.h>

using std::array;

// NOLINTNEXTLINE
TEST(sandbox_external, sigpipe_send_recv) {
    sigset_t ss;
    ASSERT_EQ(sigemptyset(&ss), 0);
    ASSERT_EQ(sigaddset(&ss, SIGPIPE), 0);
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    ASSERT_EQ(sigprocmask(SIG_BLOCK, &ss, nullptr), 0);
    auto pending_sigpipes_num = [&]() -> size_t {
        for (size_t i = 0;; ++i) {
            timespec ts = {.tv_sec = 0, .tv_nsec = 0};
            auto rc = sigtimedwait(&ss, nullptr, &ts);
            if (rc == SIGPIPE) {
                continue;
            }
            throw_assert(rc == -1 && errno == EAGAIN);
            return i;
        }
    };

    ASSERT_EQ(pending_sigpipes_num(), 0);
    int sock_fds[2];
    array<char, 1> buff = {{'x'}};
    // SOCK_STREAM
    {
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sock_fds), 0);
        ASSERT_EQ(close(sock_fds[1]), 0);

        ASSERT_TRUE(send(*sock_fds, buff.data(), buff.size(), 0) == -1 && errno == EPIPE);
        ASSERT_EQ(pending_sigpipes_num(), 1);

        ASSERT_TRUE(
            send(*sock_fds, buff.data(), buff.size(), MSG_NOSIGNAL) == -1 && errno == EPIPE
        );
        ASSERT_EQ(pending_sigpipes_num(), 0);

        ASSERT_EQ(recv(*sock_fds, buff.data(), buff.size(), MSG_DONTWAIT), 0);
        ASSERT_EQ(pending_sigpipes_num(), 0);

        ASSERT_EQ(recv(*sock_fds, buff.data(), buff.size(), MSG_DONTWAIT | MSG_NOSIGNAL), 0);
        ASSERT_EQ(pending_sigpipes_num(), 0);

        ASSERT_EQ(close(sock_fds[0]), 0);
    }
    // SOCK_SEQPACKET
    {
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sock_fds), 0);
        ASSERT_EQ(close(sock_fds[1]), 0);

        ASSERT_TRUE(send(*sock_fds, buff.data(), buff.size(), 0) == -1 && errno == EPIPE);
        ASSERT_EQ(pending_sigpipes_num(), 0);

        ASSERT_TRUE(
            send(*sock_fds, buff.data(), buff.size(), MSG_NOSIGNAL) == -1 && errno == EPIPE
        );
        ASSERT_EQ(pending_sigpipes_num(), 0);

        ASSERT_EQ(recv(*sock_fds, buff.data(), buff.size(), MSG_DONTWAIT), 0);
        ASSERT_EQ(pending_sigpipes_num(), 0);

        ASSERT_EQ(recv(*sock_fds, buff.data(), buff.size(), MSG_DONTWAIT | MSG_NOSIGNAL), 0);
        ASSERT_EQ(pending_sigpipes_num(), 0);

        ASSERT_EQ(close(sock_fds[0]), 0);
    }
    // SOCK_DGRAM
    {
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0, sock_fds), 0);
        ASSERT_EQ(close(sock_fds[1]), 0);

        ASSERT_TRUE(send(*sock_fds, buff.data(), buff.size(), 0) == -1 && errno == ECONNREFUSED);
        ASSERT_TRUE(send(*sock_fds, buff.data(), buff.size(), 0) == -1 && errno == ENOTCONN);
        ASSERT_EQ(pending_sigpipes_num(), 0);

        ASSERT_TRUE(
            send(*sock_fds, buff.data(), buff.size(), MSG_NOSIGNAL) == -1 && errno == ENOTCONN
        );
        ASSERT_EQ(pending_sigpipes_num(), 0);

        ASSERT_TRUE(
            recv(*sock_fds, buff.data(), buff.size(), MSG_DONTWAIT) == -1 && errno == EAGAIN
        );
        ASSERT_EQ(pending_sigpipes_num(), 0);

        ASSERT_TRUE(
            recv(*sock_fds, buff.data(), buff.size(), MSG_DONTWAIT | MSG_NOSIGNAL) == -1 &&
            errno == EAGAIN
        );
        ASSERT_EQ(pending_sigpipes_num(), 0);

        ASSERT_EQ(close(sock_fds[0]), 0);
    }
}
