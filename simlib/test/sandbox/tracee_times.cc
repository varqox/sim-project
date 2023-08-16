#include "assert_result.hh"

#include <gtest/gtest.h>
#include <simlib/sandbox/sandbox.hh>

using sandbox::result::Ok;
using std::chrono::operator""s;

// NOLINTNEXTLINE
TEST(sandbox, tracee_runtime) {
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{"/bin/true"}});
    sc.send_request({{"/bin/sleep", "0.05"}});
    auto res_var = sc.await_result();
    ASSERT_RESULT_OK(res_var, CLD_EXITED, 0);
    auto res = std::get<Ok>(res_var);
    ASSERT_GE(res.runtime, 0s);

    res_var = sc.await_result();
    ASSERT_RESULT_OK(res_var, CLD_EXITED, 0);
    res = std::get<Ok>(res_var);
    ASSERT_GE(res.runtime, 0.05s);
}
