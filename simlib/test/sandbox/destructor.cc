#include <chrono>
#include <gtest/gtest.h>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/to_string.hh>

// NOLINTNEXTLINE
TEST(sandbox, sandbox_destructs_cleanly_after_no_requests) {
    auto sc = sandbox::spawn_supervisor();
}

// NOLINTNEXTLINE
TEST(sandbox, sandbox_destructs_cleanly_after_unawaited_request) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{"/bin/true"}});
}

// NOLINTNEXTLINE
TEST(sandbox, sandbox_destructs_cleanly_after_awaited_request) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{"/bin/true"}});
    sc.await_result();
}

// NOLINTNEXTLINE
TEST(sandbox, sandbox_destructs_cleanly_without_waiting_for_uncompleted_request) {
    constexpr auto SLEEP_SECONDS = 10;
    auto start = std::chrono::system_clock::now();
    {
        auto sc = sandbox::spawn_supervisor();
        sc.send_request({{"/bin/sleep", to_string(SLEEP_SECONDS)}});
    }
    auto dur = std::chrono::system_clock::now() - start;
    ASSERT_LT(dur, std::chrono::seconds{SLEEP_SECONDS});
}
