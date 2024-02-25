#include "test_compiled_language_suite.hh"

#include <gtest/gtest.h>
#include <simlib/file_info.hh>
#include <simlib/sim/judge/language_suite/cpp_clang.hh>

using sim::judge::language_suite::Cpp_Clang;

constexpr auto test_prog_ok = R"(
#include <iostream>

int main() {
    std::cout << 42 << std::endl;
}
)";

constexpr auto test_prog_invalid = R"(
int main() {
    p;
}
)";

// NOLINTNEXTLINE
TEST(sim_judge_compiler, cpp_clang) {
    auto suite = Cpp_Clang{Cpp_Clang::Standard::Cpp17};
    ASSERT_EQ(suite.is_supported(), path_exists("/usr/bin/clang++"));
    if (suite.is_supported()) {
        test_compiled_language_suite(
            suite, test_prog_ok, test_prog_invalid, "error: use of undeclared identifier 'p'"
        );
    }
}
