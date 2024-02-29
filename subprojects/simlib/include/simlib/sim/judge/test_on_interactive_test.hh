#pragma once

#include <chrono>
#include <cstdint>
#include <simlib/file_path.hh>
#include <simlib/sim/judge/language_suite/suite.hh>
#include <simlib/sim/judge/test_report.hh>

namespace sim::judge {

struct InteractiveTestArgs {
    language_suite::Suite& compiled_program; // NOLINT
    language_suite::Suite& compiled_checker; // NOLINT
    FilePath test_input;

    struct Program {
        std::chrono::nanoseconds time_limit;
        std::chrono::nanoseconds cpu_time_limit;
        uint64_t memory_limit_in_bytes;
    } program;

    struct Checker {
        std::chrono::nanoseconds time_limit;
        std::chrono::nanoseconds cpu_time_limit;
        uint64_t memory_limit_in_bytes;
        uint64_t max_comment_len;
    } checker;
};

TestReport test_on_interactive_test(InteractiveTestArgs args);

} // namespace sim::judge
