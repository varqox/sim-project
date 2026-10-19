#include "fresh_execution.hh"
#include "run_in_fully_interpreted_language_suite.hh"

#include <gtest/gtest.h>
#include <simlib/file_info.hh>
#include <simlib/sandbox/si.hh>
#include <simlib/sim/judge/language_suite/bash.hh>

constexpr auto test_prog = "exit $1";

// NOLINTNEXTLINE
TEST(sim_judge_language_suite, bash) {
    auto suite = sim::judge::language_suite::Bash{};
    ASSERT_EQ(suite.is_supported(), path_exists("/usr/bin/bash"));
    ASSERT_TRUE(suite.is_supported());
    auto res = run_in_fully_intepreted_language_suite(suite, test_prog, {{"42"}});
    ASSERT_EQ(res.si, (sandbox::Si{.code = CLD_EXITED, .status = 42}));
}

// NOLINTNEXTLINE
TEST(sim_judge_language_suite, bash_prepared_program_owns_source) {
    auto program = [] {
        auto suite = sim::judge::language_suite::Bash{};
        compile_for_fresh_execution(suite, "exit $1\n");
        auto prepared = suite.prepared_program();
        compile_for_fresh_execution(suite, "exit 17\n");
        return prepared;
    }();
    auto execution = program.start_fresh({{"42"}}, fresh_execution_options());
    ASSERT_EQ(execution.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 42}));
}
