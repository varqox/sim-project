#include "assert_result.hh"

#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/time_format_conversions.hh>
#include <stdexcept>
#include <variant>

using sandbox::result::Ok;

// NOLINTNEXTLINE
TEST(sandbox, time_limit) {
    auto sc = sandbox::spawn_supervisor();
    auto res_var = sc.await_result(
        sc.send_request({{"/bin/sleep", "10"}}, {.time_limit = std::chrono::seconds{0}})
    );
    ASSERT_RESULT_OK(res_var, CLD_KILLED, SIGKILL);
    auto res = std::get<Ok>(res_var);
    ASSERT_GE(res.runtime, std::chrono::seconds{0}) << to_string(res.runtime).c_str();

    res_var = sc.await_result(
        sc.send_request({{"/bin/sleep", "10"}}, {.time_limit = std::chrono::milliseconds{10}})
    );
    ASSERT_RESULT_OK(res_var, CLD_KILLED, SIGKILL);
    res = std::get<Ok>(res_var);
    ASSERT_GE(res.runtime, std::chrono::milliseconds{10}) << to_string(res.runtime).c_str();

    // Successful run without exceeding the limit
    res_var =
        sc.await_result(sc.send_request({{"/bin/true"}}, {.time_limit = std::chrono::seconds{1}}));
    ASSERT_RESULT_OK(res_var, CLD_EXITED, 0);
    res = std::get<Ok>(res_var);
    ASSERT_LT(res.runtime, std::chrono::seconds{1}) << to_string(res.runtime).c_str();
}

// NOLINTNEXTLINE
TEST(sandbox, invalid_time_limit) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_THAT(
        [&] {
            (void)sc.send_request({{"/bin/true"}}, {.time_limit = std::chrono::nanoseconds{-1}});
        },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("invalid time limit - it has to be non-negative")
        )
    );
}
