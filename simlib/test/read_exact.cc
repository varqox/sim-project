#include "unix_socketpair.hh"

#include <gtest/gtest.h>
#include <simlib/read_exact.hh>
#include <sys/socket.h>

// NOLINTNEXTLINE
TEST(read_exact, zero_bytes) { ASSERT_EQ(read_exact(-1, nullptr, 0), 0); }

// NOLINTNEXTLINE
TEST(read_exact, partial_reads) {
    auto usock = unix_socketpair(SOCK_SEQPACKET | SOCK_CLOEXEC);
    ASSERT_EQ(write(usock.other_end, "abc", 3), 3);
    ASSERT_EQ(write(usock.other_end, "x", 1), 1);
    ASSERT_EQ(write(usock.other_end, "yz", 2), 2);
    char buff[6];
    ASSERT_EQ(read_exact(usock.our_end, buff, sizeof(buff)), 0);
    ASSERT_EQ(StringView(buff, sizeof(buff)), "abcxyz");
}

TEST(read_exact, incomplete) {
    auto usock = unix_socketpair(SOCK_SEQPACKET | SOCK_CLOEXEC);
    ASSERT_EQ(write(usock.other_end, "abc", 3), 3);
    ASSERT_EQ(usock.other_end.close(), 0);
    char buff[4];
    ASSERT_EQ(read_exact(usock.our_end, buff, sizeof(buff)), -1);
    ASSERT_EQ(errno, EPIPE);
}

TEST(read_exact, invalid_fd) {
    char buff;
    ASSERT_EQ(read_exact(-1, &buff, sizeof(buff)), -1);
    ASSERT_EQ(errno, EBADF);
}
