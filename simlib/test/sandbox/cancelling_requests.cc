#include "assert_result.hh"

#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simlib/pipe.hh>
#include <simlib/random.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/throw_assert.hh>
#include <stdexcept>
#include <thread>

// NOLINTNEXTLINE
TEST(sandbox, cancelling_works) {
    auto sc = sandbox::spawn_supervisor();
    auto pipe = pipe2(O_CLOEXEC);
    throw_assert(pipe);
    auto rh = sc.send_request(
        {{"/bin/sh", "-c", "/bin/sleep 10; echo"}},
        {
            .stdout_fd = pipe->writable,
        }
    );
    ASSERT_EQ(pipe->writable.close(), 0);
    // Cancel the request randomly in time
    std::this_thread::sleep_for(std::chrono::nanoseconds{get_random(0, 10'000'000)});
    rh.cancel();
    // Check that the request was cancelled
    char buff;
    ASSERT_EQ(read(pipe->readable, &buff, sizeof(buff)), 0);
}

// NOLINTNEXTLINE
TEST(sandbox, destructing_request_handle_cancels_request) {
    auto sc = sandbox::spawn_supervisor();
    auto pipe = pipe2(O_CLOEXEC);
    throw_assert(pipe);
    {
        auto rh = sc.send_request(
            {{"/bin/sh", "-c", "/bin/sleep 10; echo"}},
            {
                .stdout_fd = pipe->writable,
            }
        );
        ASSERT_EQ(pipe->writable.close(), 0);
        // Cancel the second request randomly in time
        std::this_thread::sleep_for(std::chrono::nanoseconds{get_random(0, 10'000'000)});
    }
    // Check that the request was cancelled
    char buff;
    ASSERT_EQ(read(pipe->readable, &buff, sizeof(buff)), 0);
}

// NOLINTNEXTLINE
TEST(sandbox, subsequent_cancels_are_noop) {
    auto sc = sandbox::spawn_supervisor();
    auto rh = sc.send_request({{"/bin/sleep", "10"}});
    rh.cancel();
    rh.cancel();
    rh.cancel();
}

// NOLINTNEXTLINE
TEST(sandbox, cancelling_done_request_is_noop) {
    auto sc = sandbox::spawn_supervisor();
    auto rh = sc.send_request({{"/bin/true"}});
    ASSERT_RESULT_OK(sc.await_result(sc.send_request({{"/bin/false"}})), CLD_EXITED, 1);
    rh.cancel();
}

// NOLINTNEXTLINE
TEST(sandbox, requests_and_cancelling_after_cancelling_request_work) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{"/bin/sleep", "10"}}).cancel();
    sc.send_request({{"/bin/sleep", "infinity"}}).cancel();
    ASSERT_RESULT_OK(sc.await_result(sc.send_request({{"/bin/true"}})), CLD_EXITED, 0);
    ASSERT_RESULT_OK(sc.await_result(sc.send_request({{"/bin/false"}})), CLD_EXITED, 1);
}

// NOLINTNEXTLINE
TEST(sandbox, killing_after_cancelling_request_works) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{"/bin/sleep", "10"}}).cancel();
    auto rh = sc.send_request({{"/bin/sleep", "10"}});
    rh.get_kill_handle().kill();
    ASSERT_RESULT_OK(sc.await_result(std::move(rh)), CLD_KILLED, SIGKILL);
}

// NOLINTNEXTLINE
TEST(sandbox, cancelling_does_not_interfere_with_other_requests) {
    auto sc = sandbox::spawn_supervisor();
    auto rh1 = sc.send_request({{"/bin/true"}});

    auto pipe = pipe2(O_CLOEXEC);
    throw_assert(pipe);
    auto rh2 = sc.send_request(
        {{"/bin/sh", "-c", "/bin/sleep 10; echo"}},
        {
            .stdout_fd = pipe->writable,
        }
    );
    ASSERT_EQ(pipe->writable.close(), 0);
    auto rh3 = sc.send_request({{"/bin/true"}});
    auto rh4 = sc.send_request({{"/bin/true"}});
    auto rh5 = sc.send_request({{"/bin/false"}});

    rh4.cancel();
    // Cancel the second request randomly in time
    std::this_thread::sleep_for(std::chrono::nanoseconds{get_random(0, 10'000'000)});
    rh2.cancel();
    // Check that the second request was cancelled
    char buff;
    ASSERT_EQ(read(pipe->readable, &buff, sizeof(buff)), 0);

    ASSERT_RESULT_OK(sc.await_result(std::move(rh1)), CLD_EXITED, 0);
    ASSERT_RESULT_OK(sc.await_result(std::move(rh3)), CLD_EXITED, 0);
    ASSERT_RESULT_OK(sc.await_result(std::move(rh5)), CLD_EXITED, 1);
}

// NOLINTNEXTLINE
TEST(sandbox, awaiting_cancelled_request_is_bug) {
    auto sc = sandbox::spawn_supervisor();
    auto rh = sc.send_request({{"/bin/sleep", "10"}});
    rh.cancel();
    ASSERT_THAT(
        [&] { sc.await_result(std::move(rh)); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("unable to await request that is cancelled")
        )
    );
}
