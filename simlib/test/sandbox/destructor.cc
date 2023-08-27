#include <chrono>
#include <gtest/gtest.h>
#include <simlib/concat_tostr.hh>
#include <simlib/sandbox/sandbox.hh>

// NOLINTNEXTLINE
TEST(sandbox, sandbox_destructs_cleanly_after_no_requests) {
    auto sc = sandbox::spawn_supervisor();
}

// NOLINTNEXTLINE
TEST(sandbox, sandbox_destructs_cleanly_after_unawaited_request) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request("");
}

// NOLINTNEXTLINE
TEST(sandbox, sandbox_destructs_cleanly_after_awaited_request) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request("");
    sc.await_result();
}

// NOLINTNEXTLINE
TEST(sandbox, sandbox_destructs_cleanly_without_waiting_for_uncompleted_request) {
    constexpr auto SLEEP_SECONDS = 10;
    auto start = std::chrono::system_clock::now();
    {
        auto sc = sandbox::spawn_supervisor();
        sc.send_request(concat_tostr("sleep ", SLEEP_SECONDS));
    }
    auto dur = std::chrono::system_clock::now() - start;
    ASSERT_LT(dur, std::chrono::seconds{SLEEP_SECONDS});
}
