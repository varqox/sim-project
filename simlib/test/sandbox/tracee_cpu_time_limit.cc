#include "../gtest_with_tester.hh"
#include "assert_result.hh"

#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/time_format_conversions.hh>
#include <stdexcept>
#include <thread>
#include <variant>

using sandbox::result::Ok;

// NOLINTNEXTLINE
TEST(sandbox, cpu_time_limit_no_process_num_limit) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{tester_executable_path, "1"}}, {.cpu_time_limit = std::chrono::seconds{0}});
    auto res_var = sc.await_result();
    ASSERT_RESULT_OK(res_var, CLD_KILLED, SIGKILL);
    auto res = std::get<Ok>(res_var);
    ASSERT_GE(res.cgroup.cpu_time.total(), std::chrono::seconds{0});
    ASSERT_LE(res.cgroup.cpu_time.total(), res.runtime)
        << "\n  " << to_string(res.cgroup.cpu_time.total()).c_str() << "\n  "
        << to_string(res.runtime).c_str();

    sc.send_request(
        {{tester_executable_path, to_string(std::thread::hardware_concurrency())}},
        {.cpu_time_limit = std::chrono::milliseconds{20}}
    );
    res_var = sc.await_result();
    ASSERT_RESULT_OK(res_var, CLD_KILLED, SIGKILL);
    res = std::get<Ok>(res_var);
    ASSERT_GE(res.cgroup.cpu_time.total(), std::chrono::milliseconds{20});

    sc.send_request(
        {{tester_executable_path, "1"}}, {.cpu_time_limit = std::chrono::milliseconds{20}}
    );
    res_var = sc.await_result();
    ASSERT_RESULT_OK(res_var, CLD_KILLED, SIGKILL);
    res = std::get<Ok>(res_var);
    ASSERT_GE(res.cgroup.cpu_time.total(), std::chrono::milliseconds{20});
    ASSERT_LE(res.cgroup.cpu_time.total(), res.runtime)
        << "\n  " << to_string(res.cgroup.cpu_time.total()).c_str() << "\n  "
        << to_string(res.runtime).c_str();

    // Successful run without exceeding the limit
    sc.send_request({{"/bin/true"}}, {.cpu_time_limit = std::chrono::seconds{1}});
    res_var = sc.await_result();
    ASSERT_RESULT_OK(res_var, CLD_EXITED, 0);
    res = std::get<Ok>(res_var);
    ASSERT_LT(res.cgroup.cpu_time.total(), std::chrono::seconds{1});
    ASSERT_LE(res.cgroup.cpu_time.total(), res.runtime)
        << "\n  " << to_string(res.cgroup.cpu_time.total()).c_str() << "\n  "
        << to_string(res.runtime).c_str();
}

// NOLINTNEXTLINE
TEST(sandbox, cpu_time_limit_process_num_limit_1) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request(
        {{tester_executable_path, "1"}},
        {
            .cgroup = {.process_num_limit = 1},
            .cpu_time_limit = std::chrono::seconds{0},
        }
    );
    auto res_var = sc.await_result();
    ASSERT_RESULT_OK(res_var, CLD_KILLED, SIGKILL);
    auto res = std::get<Ok>(res_var);
    ASSERT_GE(res.cgroup.cpu_time.total(), std::chrono::seconds{0});
    ASSERT_LE(res.cgroup.cpu_time.total(), res.runtime)
        << "\n  " << to_string(res.cgroup.cpu_time.total()).c_str() << "\n  "
        << to_string(res.runtime).c_str();

    sc.send_request(
        {{tester_executable_path, "1"}},
        {
            .cgroup = {.process_num_limit = 1},
            .cpu_time_limit = std::chrono::milliseconds{20},
        }
    );
    res_var = sc.await_result();
    ASSERT_RESULT_OK(res_var, CLD_KILLED, SIGKILL);
    res = std::get<Ok>(res_var);
    ASSERT_GE(res.cgroup.cpu_time.total(), std::chrono::milliseconds{20});
    ASSERT_LE(res.cgroup.cpu_time.total(), res.runtime)
        << "\n  " << to_string(res.cgroup.cpu_time.total()).c_str() << "\n  "
        << to_string(res.runtime).c_str();

    // Successful run without exceeding the limit
    sc.send_request(
        {{"/bin/true"}},
        {
            .cgroup = {.process_num_limit = 1},
            .cpu_time_limit = std::chrono::seconds{1},
        }
    );
    res_var = sc.await_result();
    ASSERT_RESULT_OK(res_var, CLD_EXITED, 0);
    res = std::get<Ok>(res_var);
    ASSERT_LT(res.cgroup.cpu_time.total(), std::chrono::seconds{1});
    ASSERT_LE(res.cgroup.cpu_time.total(), res.runtime)
        << "\n  " << to_string(res.cgroup.cpu_time.total()).c_str() << "\n  "
        << to_string(res.runtime).c_str();
}

// NOLINTNEXTLINE
TEST(sandbox, invalid_cpu_time_limit) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_THAT(
        [&] { sc.send_request({{"/bin/true"}}, {.cpu_time_limit = std::chrono::nanoseconds{-1}}); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("invalid cpu time limit - it has to be non-negative")
        )
    );
}
