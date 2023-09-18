#include "signal_counter.hh"

#include <array>
#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <simlib/pipe.hh>
#include <simlib/write_exact.hh>
#include <string>
#include <sys/eventfd.h>
#include <sys/socket.h>

// NOLINTNEXTLINE
TEST(write_exact, zero_bytes) { ASSERT_EQ(write_exact(-1, nullptr, 0), 0); }

// NOLINTNEXTLINE
TEST(write_exact, partial_writes) {
    int efd = eventfd(0, EFD_CLOEXEC);
    auto arr = std::array<uint64_t, 3>{{1, 2, 3}};
    ASSERT_EQ(write_exact(efd, arr.data(), arr.size() * sizeof(arr[0])), 0);
    uint64_t val = 0;
    throw_assert(read(efd, &val, sizeof(val)) == sizeof(val));
    ASSERT_EQ(val, 6);
}

// NOLINTNEXTLINE
TEST(write_exact, closed_socket) {
    auto sc = SignalCounter{SIGPIPE};
    auto pipe = pipe2(O_CLOEXEC);
    throw_assert(pipe);
    throw_assert(pipe->readable.close() == 0);
    ASSERT_EQ(write_exact(pipe->writable, "abc", 3), -1);
    ASSERT_EQ(errno, EPIPE);
    ASSERT_EQ(sc.get_count(), 1);
}

TEST(write_exact, invalid_fd) {
    char buff = 'x';
    ASSERT_EQ(write_exact(-1, &buff, sizeof(buff)), -1);
    ASSERT_EQ(errno, EBADF);
}
