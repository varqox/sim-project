#include "signal_counter.hh"

#include <array>
#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <simlib/pipe.hh>
#include <simlib/write_as_bytes.hh>
#include <string>
#include <sys/eventfd.h>
#include <sys/socket.h>

// NOLINTNEXTLINE
TEST(write_as_bytes, zero_bytes) { ASSERT_EQ(write_as_bytes(-1), 0); }

// NOLINTNEXTLINE
TEST(write_as_bytes, partial_writes) {
    int efd = eventfd(0, EFD_CLOEXEC);
    auto arr = std::array<uint64_t, 3>{{1, 2, 3}};
    ASSERT_EQ(write_as_bytes(efd, arr[0], arr[1], arr[2]), 0);
    uint64_t val = 0;
    throw_assert(read(efd, &val, sizeof(val)) == sizeof(val));
    ASSERT_EQ(val, 6);
}

// NOLINTNEXTLINE
TEST(write_as_bytes, closed_socket) {
    auto sc = SignalCounter{SIGPIPE};
    auto pipe = pipe2(O_CLOEXEC);
    throw_assert(pipe);
    throw_assert(pipe->readable.close() == 0);
    char c = 'a';
    ASSERT_EQ(write_as_bytes(pipe->writable, c), -1);
    ASSERT_EQ(errno, EPIPE);
    ASSERT_EQ(sc.get_count(), 1);
}

TEST(write_as_bytes, invalid_fd) {
    char c = 'x';
    ASSERT_EQ(write_as_bytes(-1, &c, sizeof(c)), -1);
    ASSERT_EQ(errno, EBADF);
}

// NOLINTNEXTLINE
TEST(write_as_bytes, simple) {
    auto pipe = pipe2(O_CLOEXEC);
    throw_assert(pipe);
    ASSERT_EQ(
        write_as_bytes(
            pipe->writable, static_cast<uint8_t>(0x42), static_cast<uint32_t>(0xdeadbeef)
        ),
        0
    );
    throw_assert(pipe->writable.close() == 0);

    uint8_t x;
    uint32_t y;
    std::byte buff[sizeof(x) + sizeof(y)];
    throw_assert(read(pipe->readable, buff, sizeof(buff)) == sizeof(buff));
    std::memcpy(&x, buff, sizeof(x));
    std::memcpy(&y, buff + sizeof(x), sizeof(y));
    ASSERT_EQ(x, 0x42);
    ASSERT_EQ(y, 0xdeadbeef);
    ASSERT_EQ(read(pipe->readable, buff, 1), 0); // no extra bytes were sent
}
