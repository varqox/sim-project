#include "unix_socketpair.hh"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <simlib/pipe.hh>
#include <simlib/read_bytes_as.hh>
#include <sys/socket.h>

// NOLINTNEXTLINE
TEST(read_bytes_as, zero_bytes) { ASSERT_EQ(read_bytes_as(-1), 0); }

// NOLINTNEXTLINE
TEST(read_bytes_as, partial_reads) {
    auto usock = unix_socketpair(SOCK_SEQPACKET | SOCK_CLOEXEC);
    ASSERT_EQ(write(usock.other_end, "abc", 3), 3);
    ASSERT_EQ(write(usock.other_end, "x", 1), 1);
    ASSERT_EQ(write(usock.other_end, "yz", 2), 2);
    char buff[6];
    ASSERT_EQ(
        read_bytes_as(usock.our_end, buff[0], buff[1], buff[2], buff[3], buff[4], buff[5]), 0
    );
    ASSERT_EQ(StringView(buff, sizeof(buff)), "abcxyz");
}

TEST(read_bytes_as, incomplete) {
    auto usock = unix_socketpair(SOCK_SEQPACKET | SOCK_CLOEXEC);
    ASSERT_EQ(write(usock.other_end, "abc", 3), 3);
    ASSERT_EQ(usock.other_end.close(), 0);
    char buff[4];
    ASSERT_EQ(read_bytes_as(usock.our_end, buff[0], buff[1], buff[2], buff[3]), -1);
    ASSERT_EQ(errno, EPIPE);
}

TEST(read_bytes_as, invalid_fd) {
    char c;
    ASSERT_EQ(read_bytes_as(-1, c), -1);
    ASSERT_EQ(errno, EBADF);
}

TEST(read_bytes_as, simple) {
    auto pipe = pipe2(O_CLOEXEC);
    throw_assert(pipe);
    uint8_t x = 0x42;
    uint32_t y = 0xdeadbeef;
    std::byte buff[sizeof(x) + sizeof(y)];
    std::memcpy(buff, &x, sizeof(x));
    std::memcpy(buff + sizeof(x), &y, sizeof(y));
    throw_assert(write(pipe->writable, buff, sizeof(buff)) == sizeof(buff));
    x = 0;
    y = 0;
    ASSERT_EQ(read_bytes_as(pipe->readable, x, y), 0);
    ASSERT_EQ(x, 0x42);
    ASSERT_EQ(y, 0xdeadbeef);
}
