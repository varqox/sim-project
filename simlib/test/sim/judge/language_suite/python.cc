#include "run_in_fully_interpreted_language_suite.hh"

#include <gtest/gtest.h>
#include <simlib/sandbox/si.hh>
#include <simlib/sim/judge/language_suite/python.hh>

constexpr auto test_prog = R"(
import sys
sys.exit(int(sys.argv[1]))
)";

// NOLINTNEXTLINE
TEST(sim_judge_language_suite, python) {
    auto suite = sim::judge::language_suite::Python{};
    ASSERT_TRUE(suite.is_supported());
    auto res = run_in_fully_intepreted_language_suite(suite, test_prog, {{"42"}});
    ASSERT_EQ(res.si, (sandbox::Si{.code = CLD_EXITED, .status = 42}));
}
