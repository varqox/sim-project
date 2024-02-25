#include "assert_result.hh"

#include <gtest/gtest.h>
#include <simlib/pipe.hh>
#include <simlib/random.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/throw_assert.hh>
#include <thread>

// NOLINTNEXTLINE
TEST(sandbox, killing_request_works) {
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
    // Kill the request randomly in time
    std::this_thread::sleep_for(std::chrono::nanoseconds{get_random(0, 10'000'000)});
    rh.get_kill_handle().kill();
    // Check that the request was killed
    ASSERT_RESULT_OK(sc.await_result(std::move(rh)), CLD_KILLED, SIGKILL);
    char buff;
    ASSERT_EQ(read(pipe->readable, &buff, sizeof(buff)), 0);
}

// NOLINTNEXTLINE
TEST(sandbox, subsequent_kills_are_noop) {
    auto sc = sandbox::spawn_supervisor();
    auto rh = sc.send_request({{"/bin/sleep", "10"}});
    auto krh = rh.get_kill_handle();
    krh.kill();
    krh.kill();
    krh.kill();
    ASSERT_RESULT_OK(sc.await_result(std::move(rh)), CLD_KILLED, SIGKILL);
}

// NOLINTNEXTLINE
TEST(sandbox, killing_done_request_is_noop) {
    auto sc = sandbox::spawn_supervisor();
    auto rh = sc.send_request({{"/bin/true"}});
    ASSERT_RESULT_OK(sc.await_result(sc.send_request({{"/bin/false"}})), CLD_EXITED, 1);
    rh.get_kill_handle().kill();
    ASSERT_RESULT_OK(sc.await_result(std::move(rh)), CLD_EXITED, 0);
}

// NOLINTNEXTLINE
TEST(sandbox, requests_and_cancelling_and_killing_after_killing_request_work) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{"/bin/sleep", "10"}}).get_kill_handle().kill();

    auto rh = sc.send_request({{"/bin/sleep", "10"}});
    rh.get_kill_handle().kill();
    ASSERT_RESULT_OK(sc.await_result(std::move(rh)), CLD_KILLED, SIGKILL);

    sc.send_request({{"/bin/sleep", "infinity"}}).cancel();
    ASSERT_RESULT_OK(sc.await_result(sc.send_request({{"/bin/true"}})), CLD_EXITED, 0);
    ASSERT_RESULT_OK(sc.await_result(sc.send_request({{"/bin/false"}})), CLD_EXITED, 1);
}

// NOLINTNEXTLINE
TEST(sandbox, killing_does_not_interfere_with_other_requests) {
    auto sc = sandbox::spawn_supervisor();
    auto rh1 = sc.send_request({{"/bin/true"}});
    auto rh2 = sc.send_request({{"/bin/sleep", "10"}});
    auto rh3 = sc.send_request({{"/bin/true"}});
    auto rh4 = sc.send_request({{"/bin/sleep", "10"}});
    auto rh5 = sc.send_request({{"/bin/false"}});

    rh4.get_kill_handle().kill();
    // Kill the second request randomly in time
    std::this_thread::sleep_for(std::chrono::nanoseconds{get_random(0, 10'000'000)});
    rh2.get_kill_handle().kill();

    ASSERT_RESULT_OK(sc.await_result(std::move(rh1)), CLD_EXITED, 0);
    ASSERT_RESULT_OK(sc.await_result(std::move(rh2)), CLD_KILLED, SIGKILL);
    ASSERT_RESULT_OK(sc.await_result(std::move(rh3)), CLD_EXITED, 0);
    ASSERT_RESULT_OK(sc.await_result(std::move(rh4)), CLD_KILLED, SIGKILL);
    ASSERT_RESULT_OK(sc.await_result(std::move(rh5)), CLD_EXITED, 1);
}
