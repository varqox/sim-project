#include "../gtest_with_tester.hh"
#include "assert_result.hh"
#include "mount_operations_mount_proc_if_running_under_leak_sanitizer.hh"

#include <gtest/gtest.h>
#include <simlib/concat_tostr.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/time_format_conversions.hh>

using sandbox::result::Ok;
using std::chrono::operator""s;

// NOLINTNEXTLINE
TEST(sandbox, tracee_runtime_and_cpu_time) {
    auto sc = sandbox::spawn_supervisor();
    auto rh1 = sc.send_request({{"/bin/true"}});
    auto rh2 = sc.send_request({{"/bin/sleep", "0.05"}});
    auto res_var = sc.await_result(std::move(rh1));
    ASSERT_RESULT_OK(res_var, CLD_EXITED, 0);
    auto res = std::get<Ok>(res_var);
    ASSERT_GE(res.runtime, 0s);
    ASSERT_GE(res.cgroup.cpu_time.user, 0s);
    ASSERT_GE(res.cgroup.cpu_time.system, 0s);
    ASSERT_EQ(res.cgroup.cpu_time.total(), res.cgroup.cpu_time.user + res.cgroup.cpu_time.system);
    ASSERT_LE(res.cgroup.cpu_time.total(), res.runtime);

    res_var = sc.await_result(std::move(rh2));
    ASSERT_RESULT_OK(res_var, CLD_EXITED, 0);
    res = std::get<Ok>(res_var);
    ASSERT_GE(res.runtime, 0.05s);
    ASSERT_GE(res.cgroup.cpu_time.user, 0s);
    ASSERT_GE(res.cgroup.cpu_time.system, 0s);
    ASSERT_LT(res.cgroup.cpu_time.total(), 0.05s);
    ASSERT_EQ(res.cgroup.cpu_time.total(), res.cgroup.cpu_time.user + res.cgroup.cpu_time.system);
    ASSERT_LE(res.cgroup.cpu_time.total(), res.runtime);
}

// NOLINTNEXTLINE
TEST(sandbox, tracee_cpu_time_is_accounted) {
    auto sc = sandbox::spawn_supervisor();
    auto res_var = sc.await_result(sc.send_request(
        {{tester_executable_path}},
        {
            .stderr_fd = STDERR_FILENO,
            .linux_namespaces =
                {
                    .mount =
                        {
                            .operations =
                                mount_operations_mount_proc_if_running_under_leak_sanitizer(),
                        },
                },
        }
    ));
    ASSERT_RESULT_OK(res_var, CLD_EXITED, 0);
    auto res = std::get<Ok>(res_var);
    SCOPED_TRACE(concat_tostr("real: ", to_string(res.runtime)));
    SCOPED_TRACE(concat_tostr("user: ", to_string(res.cgroup.cpu_time.user)));
    SCOPED_TRACE(concat_tostr("sys:  ", to_string(res.cgroup.cpu_time.system)));
    SCOPED_TRACE(concat_tostr("cpu:  ", to_string(res.cgroup.cpu_time.total())));
    ASSERT_GE(res.runtime, 0.05s);
    ASSERT_GE(res.cgroup.cpu_time.user, 0s);
    ASSERT_GE(res.cgroup.cpu_time.system, 0s);
    ASSERT_GE(res.cgroup.cpu_time.total(), 0.05s);
    ASSERT_EQ(res.cgroup.cpu_time.total(), res.cgroup.cpu_time.user + res.cgroup.cpu_time.system);
    ASSERT_LE(res.cgroup.cpu_time.total(), res.runtime);
}
