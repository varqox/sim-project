#include "run_in_fully_interpreted_language_suite.hh"

#include <gtest/gtest.h>
#include <simlib/sandbox/si.hh>
#include <simlib/sim/judge/language_suite/bash.hh>

constexpr auto test_prog = "exit $1";

// NOLINTNEXTLINE
TEST(sim_judge_language_suite, bash) {
    auto suite = sim::judge::language_suite::Bash{};
    ASSERT_TRUE(suite.is_supported());
    auto res = run_in_fully_intepreted_language_suite(suite, test_prog, {{"42"}});
    ASSERT_EQ(res.si, (sandbox::Si{.code = CLD_EXITED, .status = 42}));
}
