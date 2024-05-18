#include "signal_counter.hh"

#include <climits>
#include <cstdint>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <pthread.h>
#include <simlib/defer.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/pipe.hh>
#include <simlib/random.hh>
#include <simlib/read_file.hh>
#include <simlib/throw_assert.hh>
#include <simlib/write_as_bytes.hh>
#include <string>
#include <string_view>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

// NOLINTNEXTLINE
TEST(write_as_bytes, zero_bytes) { ASSERT_EQ(write_as_bytes(-1), 0); }

// NOLINTNEXTLINE
TEST(write_as_bytes, partial_writes) {
    auto pipe = pipe2(O_CLOEXEC);
    throw_assert(pipe);
    ASSERT_EQ(fcntl(pipe->writable, F_SETPIPE_SZ, PIPE_BUF), PIPE_BUF);

    struct ForPartialWrites {
        char buff[PIPE_BUF * 3];
    } fpw;

    fill_randomly(fpw.buff, sizeof(fpw.buff));
    uint32_t x = 42;

    pid_t pid = fork();
    throw_assert(pid != -1);
    if (pid == 0) {
        throw_assert(pipe->readable.close() == 0);
        struct sigaction sa = {};
        sa.sa_handler = [](int) {
            if (kill(getppid(), SIGUSR2)) {
                _exit(1);
            }
        };
        throw_assert(sigaction(SIGUSR1, &sa, nullptr) == 0);
        throw_assert(write_as_bytes(pipe->writable, fpw, x) == 0);
        _exit(0);
    }

    throw_assert(pipe->writable.close() == 0);

    std::string data(PIPE_BUF, 0);
    throw_assert(
        read(pipe->readable, data.data(), data.size()) == static_cast<ssize_t>(data.size())
    );

    sigset_t ss;
    throw_assert(sigemptyset(&ss) == 0);
    throw_assert(sigaddset(&ss, SIGUSR2) == 0);
    sigset_t old_ss;
    throw_assert(pthread_sigmask(SIG_BLOCK, &ss, &old_ss) == 0);
    {
        auto cleaner =
            Defer{[&] { throw_assert(pthread_sigmask(SIG_SETMASK, &old_ss, nullptr) == 0); }};
        kill(pid, SIGUSR1);
        // Wait for the child to receive the signal
        throw_assert(sigwaitinfo(&ss, nullptr) == SIGUSR2);
    }

    data += read_file(pipe->readable);

    int status;
    throw_assert(waitpid(pid, &status, 0) == pid);
    ASSERT_EQ(status, 0);

    EXPECT_EQ(data.size(), sizeof(fpw.buff) + sizeof(x));
    EXPECT_EQ(
        std::string_view(data.data(), sizeof(fpw.buff)),
        std::string_view(fpw.buff, sizeof(fpw.buff))
    );
    EXPECT_EQ(memcmp(data.data() + sizeof(fpw.buff), &x, sizeof(x)), 0);
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
