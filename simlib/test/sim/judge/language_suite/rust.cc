#include "test_compiled_language_suite.hh"

#include <gtest/gtest.h>
#include <simlib/sim/judge/language_suite/rust.hh>

using sim::judge::language_suite::Rust;

constexpr auto test_prog_ok = R"(
fn main() {
    println!("{}", vec![1, 2, 3].len());
}
)";

constexpr auto test_prog_invalid = R"(
fn main() { p }
)";

// NOLINTNEXTLINE
TEST(sim_judge_compiler, rust) {
    auto suite = Rust{Rust::Edition::ed2021};
    if (suite.is_supported()) {
        test_compiled_language_suite(
            suite, test_prog_ok, test_prog_invalid, "cannot find value `p` in this scope"
        );
    }
}
