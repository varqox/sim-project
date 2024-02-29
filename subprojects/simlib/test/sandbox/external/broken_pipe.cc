#include <array>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <simlib/pipe.hh>
#include <simlib/throw_assert.hh>
#include <sys/socket.h>

using std::array;

// NOLINTNEXTLINE
TEST(sandbox_external, reading_from_closed_pipe) {
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

    // Reading from closed pipe
    ASSERT_EQ(pending_sigpipes_num(), 0);
    std::array<char, 16> buff;
    // No data
    {
        auto pipe_opt = pipe2(O_CLOEXEC);
        ASSERT_TRUE(pipe_opt.has_value());
        auto pipe = std::move(*pipe_opt); // NOLINT(bugprone-unchecked-optional-access)
        ASSERT_EQ(pipe.writable.close(), 0);

        ASSERT_EQ(read(pipe.readable, buff.data(), buff.size()), 0);
        ASSERT_EQ(pending_sigpipes_num(), 0);
    }
    // Some data
    {
        auto pipe_opt = pipe2(O_CLOEXEC);
        ASSERT_TRUE(pipe_opt.has_value());
        auto pipe = std::move(*pipe_opt); // NOLINT(bugprone-unchecked-optional-access)
        ASSERT_EQ(write(pipe.writable, "xxx", 3), 3);
        ASSERT_EQ(pipe.writable.close(), 0);

        ASSERT_EQ(read(pipe.readable, buff.data(), buff.size()), 3);
        ASSERT_EQ(pending_sigpipes_num(), 0);
        ASSERT_EQ(read(pipe.readable, buff.data(), buff.size()), 0);
        ASSERT_EQ(pending_sigpipes_num(), 0);
    }

    // Writing to closed pipe
    ASSERT_EQ(pending_sigpipes_num(), 0);
    // write()
    {
        auto pipe_opt = pipe2(O_CLOEXEC);
        ASSERT_TRUE(pipe_opt.has_value());
        auto pipe = std::move(*pipe_opt); // NOLINT(bugprone-unchecked-optional-access)
        ASSERT_EQ(write(pipe.writable, "xxx", 3), 3);
        ASSERT_EQ(pipe.readable.close(), 0);

        ASSERT_EQ(write(pipe.writable, "xxx", 3), -1);
        ASSERT_EQ(errno, EPIPE);
        ASSERT_EQ(pending_sigpipes_num(), 1);
    }
    // send()
    {
        auto pipe_opt = pipe2(O_CLOEXEC);
        ASSERT_TRUE(pipe_opt.has_value());
        auto pipe = std::move(*pipe_opt); // NOLINT(bugprone-unchecked-optional-access)
        ASSERT_EQ(send(pipe.writable, "xxx", 3, MSG_NOSIGNAL), -1);
        ASSERT_EQ(errno, ENOTSOCK);
        ASSERT_EQ(pending_sigpipes_num(), 0);
    }
}
