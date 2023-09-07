#include "test_compiled_language_suite.hh"

#include <gtest/gtest.h>
#include <simlib/sim/judge/language_suite/c_gcc.hh>

using sim::judge::language_suite::C_GCC;

constexpr auto test_prog_ok = R"(
#include <stdio.h>

int main() {
    printf("%i\n", 42);
}
)";

constexpr auto test_prog_invalid = R"(
int main() {
    p;
}
)";

// NOLINTNEXTLINE
TEST(sim_judge_compiler, c_gcc) {
    auto suite = C_GCC{C_GCC::Standard::C11};
    if (suite.is_supported()) {
        test_compiled_language_suite(
            suite, test_prog_ok, test_prog_invalid, "error: 'p' undeclared"
        );
    }
}
