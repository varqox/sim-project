#include "test_compiled_language_suite.hh"

#include <gtest/gtest.h>
#include <simlib/sim/judge/language_suite/cpp_gcc.hh>

using sim::judge::language_suite::Cpp_GCC;

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
TEST(sim_judge_compiler, cpp_gcc) {
    auto suite = Cpp_GCC{Cpp_GCC::Standard::Cpp17};
    if (suite.is_supported()) {
        test_compiled_language_suite(
            suite, test_prog_ok, test_prog_invalid, "error: 'p' was not declared in this scope"
        );
    }
}
