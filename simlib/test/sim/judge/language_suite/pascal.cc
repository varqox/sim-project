#include "test_compiled_language_suite.hh"

#include <gtest/gtest.h>
#include <simlib/sim/judge/language_suite/pascal.hh>

constexpr auto test_prog_ok = R"(
begin
    writeln(42);
end.
)";

constexpr auto test_prog_invalid = R"(
begin
    w;
end.
)";

// NOLINTNEXTLINE
TEST(sim_judge_compiler, pascal) {
    auto suite = sim::judge::language_suite::Pascal{};
    if (suite.is_supported()) {
        test_compiled_language_suite(
            suite, test_prog_ok, test_prog_invalid, "Error: Identifier not found \"w\""
        );
    }
}
