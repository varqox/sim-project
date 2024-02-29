#pragma once

#include <chrono>
#include <simlib/file_contents.hh>
#include <simlib/file_perms.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/sim/judge/language_suite/fully_interpreted_language.hh>
#include <simlib/slice.hh>
#include <simlib/temporary_file.hh>
#include <string_view>

inline sandbox::result::Ok run_in_fully_intepreted_language_suite(
    sim::judge::language_suite::FullyInterpretedLanguage& suite,
    StringView source,
    Slice<std::string_view> args
) {
    auto tmp_file = TemporaryFile{"/tmp/sim_judge_fully_interpreted_language_suite_test.XXXXXX"};
    put_file_contents(tmp_file.path(), source);
    // Fully interpreted language has no compilation phase
    suite.compile(
        tmp_file.path(),
        {
            .time_limit = std::chrono::seconds{0},
            .cpu_time_limit = std::chrono::seconds{0},
            .memory_limit_in_bytes = 0,
            .max_file_size_in_bytes = 0,
        }
    );

    return suite.await_result(suite.async_run(
        args,
        {
            .stdin_fd = std::nullopt,
            .stdout_fd = std::nullopt,
            .stderr_fd = std::nullopt,
            .time_limit = std::chrono::seconds{60}, // Under load it may take time.
            .cpu_time_limit = std::chrono::seconds{1},
            .memory_limit_in_bytes = 32 << 20,
            .max_stack_size_in_bytes = 32 << 20,
            .max_file_size_in_bytes = 0,
        },
        {}
    ));
}
