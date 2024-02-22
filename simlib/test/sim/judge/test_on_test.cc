#include "compile.hh"
#include "simlib/utilities.hh"

#include <chrono>
#include <gtest/gtest.h>
#include <optional>
#include <simlib/file_contents.hh>
#include <simlib/file_path.hh>
#include <simlib/overloaded.hh>
#include <simlib/sim/judge/language_suite/bash.hh>
#include <simlib/sim/judge/language_suite/python.hh>
#include <simlib/sim/judge/test_on_test.hh>
#include <simlib/string_view.hh>
#include <simlib/temporary_file.hh>
#include <variant>

using sim::judge::TestReport;
using sim::judge::language_suite::Suite;
using Status = sim::judge::TestReport::Status;

struct Python {
    StringView source;
};

struct Bash {
    StringView source;
};

using Source = std::variant<Python, Bash>;

struct TestOnTestOptions {
    std::optional<FilePath> test_input = std::nullopt;
    std::optional<FilePath> expected_output = std::nullopt;
    std::chrono::nanoseconds program_cpu_time_limit = std::chrono::milliseconds{100};
    uint64_t output_size_limit = 0;
    std::chrono::nanoseconds checker_cpu_time_limit = std::chrono::milliseconds{100};
};

TestReport test_test_on_test(Source prog, Source checker, const TestOnTestOptions& options = {}) {
    static auto prog_python = sim::judge::language_suite::Python{};
    static auto prog_bash = sim::judge::language_suite::Bash{};
    static auto checker_python = sim::judge::language_suite::Python{};
    static auto checker_bash = sim::judge::language_suite::Bash{};

    auto empty_file = TemporaryFile{"/tmp/sim_judge_test_on_test_empty.XXXXXX"};
    return sim::judge::test_on_test({
        .compiled_program = [&]() -> Suite& {
            StringView source = std::visit([](auto x) { return x.source; }, prog);
            auto& suite = std::visit(
                overloaded{
                    [&](const Python& /**/) -> Suite& { return prog_python; },
                    [&](const Bash& /**/) -> Suite& { return prog_bash; },
                },
                prog
            );
            compile(suite, source);
            return suite;
        }(),
        .compiled_checker = [&]() -> Suite& {
            StringView source = std::visit([](auto x) { return x.source; }, checker);
            auto& suite = std::visit(
                overloaded{
                    [&](const Python& /**/) -> Suite& { return checker_python; },
                    [&](const Bash& /**/) -> Suite& { return checker_bash; },
                },
                checker
            );
            compile(suite, source);
            return suite;
        }(),
        .test_input = options.test_input.value_or(empty_file.path()),
        .expected_output = options.expected_output.value_or(empty_file.path()),
        .program =
            {
                .time_limit = std::chrono::seconds{10},
                .cpu_time_limit = options.program_cpu_time_limit,
                .memory_limit_in_bytes = 16 << 20,
                .output_size_limit_in_bytes = options.output_size_limit,
            },
        .checker =
            {
                .time_limit = std::chrono::seconds{10},
                .cpu_time_limit = options.checker_cpu_time_limit,
                .memory_limit_in_bytes = 16 << 20,
                .max_comment_len = 512,
            },
    });
}

constexpr auto prog_ok = Bash{""};
constexpr auto prog_tle = Bash{"while :; do :; done"};
constexpr auto prog_mle = Python{R"(s = 'x'
while True: s += s)"};
constexpr auto prog_rte = Bash{"exit 1"};
constexpr auto checker_tle = prog_tle;
constexpr auto checker_mle = prog_mle;
constexpr auto checker_rte = prog_rte;
constexpr auto checker_ok_no_comment = Bash{"printf OK >&2"};
constexpr auto checker_ok_score_and_comment =
    Bash{R"(printf "OK\n42.3\nsome comment\neven with\nnewlines" >&2)"};
constexpr auto checker_wrong_no_comment = Bash{"echo WRONG >&2"};
constexpr auto checker_wrong_and_comment =
    Bash{R"(printf "WRONG\n0\nsome comment\neven with\nnewlines" >&2)"};

// NOLINTNEXTLINE
TEST(test_on_test, prog_tle) {
    auto report = test_test_on_test(
        prog_tle, checker_rte, {.program_cpu_time_limit = std::chrono::seconds{0}}
    );
    EXPECT_EQ(report.status, Status::TimeLimitExceeded);
    EXPECT_EQ(report.comment, "");
    EXPECT_EQ(report.score, 0);
}

// NOLINTNEXTLINE
TEST(test_on_test, prog_mle) {
    auto report = test_test_on_test(prog_mle, checker_rte);
    EXPECT_EQ(report.status, Status::MemoryLimitExceeded);
    EXPECT_EQ(report.comment, "");
    EXPECT_EQ(report.score, 0);
}

constexpr auto prog_output_size_limit_exceeded = Bash{"printf abc"};

// NOLINTNEXTLINE
TEST(test_on_test, prog_output_size_limit_exceeded) {
    auto report =
        test_test_on_test(prog_output_size_limit_exceeded, checker_rte, {.output_size_limit = 2});
    EXPECT_EQ(report.status, Status::OutputSizeLimitExceeded);
    EXPECT_EQ(report.comment, "");
    EXPECT_EQ(report.score, 0);
}

// NOLINTNEXTLINE
TEST(test_on_test, prog_rte) {
    auto report = test_test_on_test(prog_rte, checker_rte);
    EXPECT_EQ(report.status, Status::RuntimeError);
    EXPECT_EQ(report.comment, "Runtime error: exited with 1");
    EXPECT_EQ(report.score, 0);
}

// NOLINTNEXTLINE
TEST(test_on_test, checker_tle) {
    auto report = test_test_on_test(
        prog_ok, checker_tle, {.checker_cpu_time_limit = std::chrono::seconds{0}}
    );
    EXPECT_EQ(report.status, Status::CheckerError);
    EXPECT_EQ(report.comment, "Checker error: time limit exceeded");
    EXPECT_EQ(report.score, 0);
}

// NOLINTNEXTLINE
TEST(test_on_test, checker_mle) {
    auto report = test_test_on_test(prog_ok, checker_mle);
    EXPECT_EQ(report.status, Status::CheckerError);
    EXPECT_EQ(report.comment, "Checker error: memory limit exceeded");
    EXPECT_EQ(report.score, 0);
}

// NOLINTNEXTLINE
TEST(test_on_test, checker_rte) {
    auto report = test_test_on_test(prog_ok, checker_rte);
    EXPECT_EQ(report.status, Status::CheckerError);
    EXPECT_EQ(report.comment, "Checker runtime error: exited with 1");
    EXPECT_EQ(report.score, 0);
}

// NOLINTNEXTLINE
TEST(test_on_test, prog_ok_checker_ok_no_comment) {
    auto report = test_test_on_test(prog_ok, checker_ok_no_comment);
    EXPECT_EQ(report.status, Status::OK);
    EXPECT_EQ(report.comment, "");
    EXPECT_EQ(report.score, 1);
}

// NOLINTNEXTLINE
TEST(test_on_test, prog_ok_checker_wrong_no_comment) {
    auto report = test_test_on_test(prog_ok, checker_wrong_no_comment);
    EXPECT_EQ(report.status, Status::WrongAnswer);
    EXPECT_EQ(report.comment, "");
    EXPECT_EQ(report.score, 0);
}

// NOLINTNEXTLINE
TEST(test_on_test, prog_ok_checker_ok_score_and_comment) {
    auto report = test_test_on_test(prog_ok, checker_ok_score_and_comment);
    EXPECT_EQ(report.status, Status::OK);
    EXPECT_EQ(report.comment, "some comment\neven with\nnewlines");
    EXPECT_EQ(report.score, 0.423);
}

// NOLINTNEXTLINE
TEST(test_on_test, prog_ok_checker_wrong_and_comment) {
    auto report = test_test_on_test(prog_ok, checker_wrong_and_comment);
    EXPECT_EQ(report.status, Status::WrongAnswer);
    EXPECT_EQ(report.comment, "some comment\neven with\nnewlines");
    EXPECT_EQ(report.score, 0);
}

constexpr auto checker_wrong_first_line = Bash{"echo abc >&2"};

// NOLINTNEXTLINE
TEST(test_on_test, checker_wrong_first_line) {
    auto report = test_test_on_test(prog_ok, checker_wrong_first_line);
    EXPECT_EQ(report.status, Status::CheckerError);
    EXPECT_EQ(report.comment, R"(Checker error: invalid first line (expected "OK" or "WRONG"))");
    EXPECT_EQ(report.score, 0);
}

constexpr auto checker_wrong_score_line_non_number = Bash{R"(printf "OK\nabc" >&2)"};

// NOLINTNEXTLINE
TEST(test_on_test, checker_wrong_score_line_non_number) {
    auto report = test_test_on_test(prog_ok, checker_wrong_score_line_non_number);
    EXPECT_EQ(report.status, Status::CheckerError);
    EXPECT_EQ(
        report.comment,
        "Checker error: invalid second line (expected real number in range [0, 100] or empty line)"
    );
    EXPECT_EQ(report.score, 0);
}

constexpr auto checker_wrong_score_line_too_big = Bash{R"(printf "OK\n101" >&2)"};

// NOLINTNEXTLINE
TEST(test_on_test, checker_wrong_score_line_too_big) {
    auto report = test_test_on_test(prog_ok, checker_wrong_score_line_too_big);
    EXPECT_EQ(report.status, Status::CheckerError);
    EXPECT_EQ(
        report.comment,
        "Checker error: invalid second line (expected real number in range [0, 100] or empty line)"
    );
    EXPECT_EQ(report.score, 0);
}

constexpr auto checker_wrong_score_line_too_small = Bash{R"(printf "OK\n-1" >&2)"};

// NOLINTNEXTLINE
TEST(test_on_test, checker_wrong_score_line_too_small) {
    auto report = test_test_on_test(prog_ok, checker_wrong_score_line_too_small);
    EXPECT_EQ(report.status, Status::CheckerError);
    EXPECT_EQ(
        report.comment,
        "Checker error: invalid second line (expected real number in range [0, 100] or empty line)"
    );
    EXPECT_EQ(report.score, 0);
}

constexpr auto checker_too_long_comment = Bash{R"(printf "OK\n\n%10000s" >&2)"};

// NOLINTNEXTLINE
TEST(test_on_test, checker_too_long_comment) {
    auto report = test_test_on_test(prog_ok, checker_too_long_comment);
    EXPECT_EQ(report.status, Status::CheckerError);
    EXPECT_TRUE(is_one_of(
        report.comment,
        "Checker runtime error: killed by signal XFSZ - File size limit exceeded",
        "Checker runtime error: killed and dumped by signal XFSZ - File size limit exceeded"
    )) << report.comment;
    EXPECT_EQ(report.score, 0);
}

constexpr auto checker_truncated_too_long_comment = Bash{R"(printf "OK\n50\n%513s" "ab" >&2)"};

// NOLINTNEXTLINE
TEST(test_on_test, checker_truncated_too_long_comment) {
    auto report = test_test_on_test(prog_ok, checker_truncated_too_long_comment);
    EXPECT_EQ(report.status, Status::OK);
    EXPECT_EQ(report.comment, std::string(511, ' ') + "a");
    EXPECT_EQ(report.score, 0.5);
}

constexpr auto prog_io_simple = Bash{"read x; echo $((x * 2))"};

constexpr auto checker_io_simple = Bash{R"(
read -d '' expected_output < "$2"
read -d '' prog_output < "$3"
if [ "$expected_output" == "$prog_output" ]; then
    echo "OK" >&2
fi
)"};

// NOLINTNEXTLINE
TEST(test_on_test, io_simple) {
    auto test_input = TemporaryFile{"/tmp/sim_judge_test_on_test_input.XXXXXX"};
    put_file_contents(test_input.path(), "42");

    auto test_output = TemporaryFile{"/tmp/sim_judge_test_on_test_output.XXXXXX"};
    put_file_contents(test_output.path(), "84\n");

    auto report = test_test_on_test(
        prog_io_simple,
        checker_io_simple,
        {
            .test_input = test_input.path(),
            .expected_output = test_output.path(),
            .output_size_limit = 3,
        }
    );
    EXPECT_EQ(report.status, Status::OK);
    EXPECT_EQ(report.comment, "");
    EXPECT_EQ(report.score, 1);
}

constexpr auto prog_io_is_not_seekable = Python{R"(
import sys
assert not(sys.stdin.seekable())
assert not(sys.stdout.seekable())
# stderr is /dev/null so we don't care about it being seekable
)"};

// NOLINTNEXTLINE
TEST(test_on_test, io_is_not_seekable) {
    auto test_inout = TemporaryFile{"/tmp/sim_judge_test_on_test_inout.XXXXXX"};

    auto report = test_test_on_test(
        prog_io_is_not_seekable,
        checker_ok_no_comment,
        {
            .test_input = test_inout.path(),
            .expected_output = test_inout.path(),
        }
    );
    EXPECT_EQ(report.status, Status::OK);
    EXPECT_EQ(report.comment, "");
    EXPECT_EQ(report.score, 1);
}

// Python is faster than bash here
constexpr auto prog_io_closes_partially_read_stdin_before_writing_to_stdout = Python{R"(
import sys, os
a = input()
os.close(sys.stdin.fileno())
print(a)
print(a)
)"};

// Python is faster than bash here
constexpr auto checker_io_input_equals_output = Python{R"(
import sys

with open(sys.argv[2], mode='r') as f:
    expected_output = f.read()
with open(sys.argv[3], mode='r') as f:
    prog_output = f.read()
if expected_output == prog_output:
    print("OK\n\n", file=sys.stderr)
)"};

// NOLINTNEXTLINE
TEST(test_on_test, io_closes_partially_read_stdin_before_writing_to_stdout) {
    auto test_inout = TemporaryFile{"/tmp/sim_judge_test_on_test_inout.XXXXXX"};
    put_file_contents(
        test_inout.path(),
        from_unsafe{concat_tostr(std::string(3e5, 'a'), '\n', std::string(3e5, 'a'), '\n')}
    );

    auto report = test_test_on_test(
        prog_io_closes_partially_read_stdin_before_writing_to_stdout,
        checker_io_input_equals_output,
        {
            .test_input = test_inout.path(),
            .expected_output = test_inout.path(),
            .output_size_limit = 300'000 * 2 + 2,
        }
    );
    EXPECT_EQ(report.status, Status::OK);
    EXPECT_EQ(report.comment, "");
    EXPECT_EQ(report.score, 1);
}

// Python is faster than bash here
constexpr auto prog_io_closes_stdout_and_reads_stdin_afterwards = Python{R"(
import sys, os
a = input()
print(a)
print(a)
sys.stdout.flush()
os.close(sys.stdout.fileno())
b = input()
assert a == b
)"};

// NOLINTNEXTLINE
TEST(test_on_test, io_closes_stdout_and_reads_stdin_afterwards) {
    auto test_inout = TemporaryFile{"/tmp/sim_judge_test_on_test_inout.XXXXXX"};
    put_file_contents(
        test_inout.path(),
        from_unsafe{concat_tostr(std::string(3e5, 'a'), '\n', std::string(3e5, 'a'), '\n')}
    );

    auto report = test_test_on_test(
        prog_io_closes_stdout_and_reads_stdin_afterwards,
        checker_io_input_equals_output,
        {
            .test_input = test_inout.path(),
            .expected_output = test_inout.path(),
            .output_size_limit = 300'000 * 2 + 2,
        }
    );
    EXPECT_EQ(report.status, Status::OK);
    EXPECT_EQ(report.comment, "");
    EXPECT_EQ(report.score, 1);
}
