#include "signal_counter.hh"

#include <array>
#include <climits>
#include <csignal>
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
#include <simlib/write_exact.hh>
#include <string>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

// NOLINTNEXTLINE
TEST(write_exact, zero_bytes) { ASSERT_EQ(write_exact(-1, nullptr, 0), 0); }

// NOLINTNEXTLINE
TEST(write_exact, partial_writes) {
    auto pipe = pipe2(O_CLOEXEC);
    throw_assert(pipe);
    ASSERT_EQ(fcntl(pipe->writable, F_SETPIPE_SZ, PIPE_BUF), PIPE_BUF);

    std::array<char, PIPE_BUF * 3> buff;
    fill_randomly(buff.data(), buff.size());

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
        throw_assert(write_exact(pipe->writable, buff.data(), buff.size()) == 0);
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

    EXPECT_EQ(data, std::string_view(buff.data(), buff.size()));
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
