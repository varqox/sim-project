#pragma once

#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simlib/file_contents.hh>
#include <simlib/sim/judge/language_suite/suite.hh>
#include <simlib/string_view.hh>
#include <simlib/temporary_file.hh>
#include <sys/wait.h>

inline void test_compiled_language_suite(
    sim::judge::language_suite::Suite& suite,
    StringView ok_source,
    StringView invalid_source,
    std::string_view compilation_errors_regex
) {
    auto source_file = TemporaryFile{"/tmp/sim_judge_compiled_language_suite_test.XXXXXX"};
    auto compiler_options = sim::judge::language_suite::Suite::CompileOptions{
        .time_limit = std::chrono::seconds{20},
        .cpu_time_limit = std::chrono::seconds{20},
        .memory_limit_in_bytes = 1 << 30,
        .max_file_size_in_bytes = 20 << 20,
    };

    put_file_contents(source_file.path(), ok_source);
    auto cres = suite.compile(source_file.path(), compiler_options);
    if (cres.is_err()) {
        FAIL() << "Compilation failed:\n" << get_file_contents(std::move(cres).unwrap_err(), 0, -1);
    }

    auto res = suite.await_result(suite.async_run(
        {},
        {
            .stdin_fd = std::nullopt,
            .stdout_fd = std::nullopt,
            .stderr_fd = std::nullopt,
            .time_limit = std::chrono::seconds{1},
            .cpu_time_limit = std::chrono::seconds{1},
            .memory_limit_in_bytes = 32 << 20,
            .max_stack_size_in_bytes = 32 << 20,
            .max_file_size_in_bytes = 0,
        },
        {}
    ));
    ASSERT_EQ(res.si, (sandbox::Si{.code = CLD_EXITED, .status = 0}));

    put_file_contents(source_file.path(), invalid_source);
    cres = suite.compile(source_file.path(), compiler_options);
    if (cres.is_ok()) {
        FAIL() << "Expected compilation error but the compilation succeeded";
    }
    auto compilation_errors = get_file_contents(std::move(cres).unwrap_err(), 0, -1);
    ASSERT_THAT(compilation_errors, testing::ContainsRegex(compilation_errors_regex));
}
