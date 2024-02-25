#include "test_compiled_language_suite.hh"

#include <gtest/gtest.h>
#include <simlib/file_info.hh>
#include <simlib/sim/judge/language_suite/c_clang.hh>

using sim::judge::language_suite::C_Clang;

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
TEST(sim_judge_compiler, c_clang) {
    auto suite = C_Clang{C_Clang::Standard::C11};
    ASSERT_EQ(suite.is_supported(), path_exists("/usr/bin/clang"));
    if (suite.is_supported()) {
        test_compiled_language_suite(
            suite, test_prog_ok, test_prog_invalid, "error: use of undeclared identifier 'p'"
        );
    }
}
