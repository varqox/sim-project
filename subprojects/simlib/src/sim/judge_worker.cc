#include "default_checker_dump.h"

#include <cmath>
#include <cstdio>
#include <fcntl.h>
#include <memory>
#include <simlib/concat.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/enum_val.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_info.hh>
#include <simlib/libzip.hh>
#include <simlib/noexcept_concat.hh>
#include <simlib/sim/judge/language_suite/c_gcc.hh>
#include <simlib/sim/judge/language_suite/cpp_gcc.hh>
#include <simlib/sim/judge/language_suite/pascal.hh>
#include <simlib/sim/judge/language_suite/python.hh>
#include <simlib/sim/judge/language_suite/rust.hh>
#include <simlib/sim/judge/language_suite/suite.hh>
#include <simlib/sim/judge/test_on_interactive_test.hh>
#include <simlib/sim/judge/test_on_test.hh>
#include <simlib/sim/judge_worker.hh>
#include <simlib/sim/problem_package.hh>
#include <simlib/simple_parser.hh>
#include <simlib/string_traits.hh>
#include <simlib/throw_assert.hh>
#include <simlib/unlinked_temporary_file.hh>
#include <simlib/working_directory.hh>
#include <sys/stat.h>
#include <unistd.h>

using std::string;

namespace sim {

class DirPackageLoader : public PackageLoader {
    InplaceBuff<PATH_MAX> pkg_root_;

public:
    explicit DirPackageLoader(FilePath pkg_path) : pkg_root_(pkg_path) {
        if (pkg_root_.size and pkg_root_.back() != '/') {
            pkg_root_.append('/');
        }
    }

    std::string load_into_dest_file(FilePath path, FilePath dest) override {
        if (copy(concat(pkg_root_, path), dest)) {
            THROW("copy()", errmsg());
        }

        return dest.to_str();
    }

    std::string load_as_file(FilePath path, FilePath /*hint_name*/) override {
        auto res = concat_tostr(pkg_root_, path);
        if (access(res, F_OK) != 0) {
            THROW("load_as_file() - Such file does not exist");
        }

        return res;
    }

    std::string load_as_str(FilePath path) override {
        return get_file_contents(concat(pkg_root_, path));
    }
};

class ZipPackageLoader : public PackageLoader {
    TemporaryDirectory& tmp_dir_; // NOLINT
    ZipFile zip_;
    std::string pkg_main_dir_;

    auto as_pkg_path(FilePath path) { return concat(pkg_main_dir_, path); }

public:
    ZipPackageLoader(TemporaryDirectory& tmp_dir, FilePath pkg_path)
    : tmp_dir_(tmp_dir)
    , zip_(pkg_path, ZIP_RDONLY)
    , pkg_main_dir_(sim::zip_package_main_dir(zip_)) {}

    std::string load_into_dest_file(FilePath path, FilePath dest) override {
        zip_.extract_to_file(zip_.get_index(as_pkg_path(path)), dest, S_0600);
        return dest.to_str();
    }

    std::string load_as_file(FilePath path, FilePath hint_name) override {
        auto dest = concat_tostr(tmp_dir_.path(), "from_zip_pgk:", hint_name);
        zip_.extract_to_file(zip_.get_index(as_pkg_path(path)), dest, S_0600);
        return dest;
    }

    std::string load_as_str(FilePath path) override {
        return zip_.extract_to_str(zip_.get_index(as_pkg_path(path)));
    }
};

std::unique_ptr<judge::language_suite::Suite> lang_to_suite(SolutionLanguage lang) {
    switch (lang) {
    case SolutionLanguage::UNKNOWN: {
        THROW("unknown programming language");
    }
    case SolutionLanguage::C11: {
        return std::make_unique<judge::language_suite::C_GCC>(
            judge::language_suite::C_GCC::Standard::C11
        );
    } break;
    case SolutionLanguage::CPP11: {
        return std::make_unique<judge::language_suite::Cpp_GCC>(
            judge::language_suite::Cpp_GCC::Standard::Cpp11
        );
    } break;
    case SolutionLanguage::CPP14: {
        return std::make_unique<judge::language_suite::Cpp_GCC>(
            judge::language_suite::Cpp_GCC::Standard::Cpp14
        );
    } break;
    case SolutionLanguage::CPP17: {
        return std::make_unique<judge::language_suite::Cpp_GCC>(
            judge::language_suite::Cpp_GCC::Standard::Cpp17
        );
    } break;
    case SolutionLanguage::CPP20: {
        return std::make_unique<judge::language_suite::Cpp_GCC>(
            judge::language_suite::Cpp_GCC::Standard::Cpp20
        );
    } break;
    case SolutionLanguage::PASCAL: {
        return std::make_unique<judge::language_suite::Pascal>();
    } break;
    case SolutionLanguage::PYTHON: {
        return std::make_unique<judge::language_suite::Python>();
    } break;
    case SolutionLanguage::RUST: {
        return std::make_unique<judge::language_suite::Rust>(
            judge::language_suite::Rust::Edition::ed2021
        );
    } break;
    }
    __builtin_unreachable();
}

JudgeWorker::JudgeWorker(JudgeWorkerOptions options)
: max_executable_size_in_bytes{options.max_executable_size_in_bytes}
, checker_time_limit{options.checker_time_limit}
, checker_memory_limit_in_bytes{options.checker_memory_limit_in_bytes}
, score_cut_lambda{options.score_cut_lambda} {
    if (score_cut_lambda < 0 or score_cut_lambda > 1) {
        THROW("score_cut_lambda has to be from [0, 1]");
    }
}

int JudgeWorker::compile_checker(
    std::chrono::nanoseconds time_limit,
    uint64_t compiler_memory_limit_in_bytes,
    std::string* c_errors,
    size_t c_errors_max_len,
    judge::CompilationCache* cache,
    std::optional<std::string> cached_name
) {
    STACK_UNWINDING_MARK;
    if (cache && !cached_name) {
        THROW("cached_name is required if cache is provided");
    }

    auto [source_path, lang] = [&]() -> std::pair<std::string, SolutionLanguage> {
        if (sf.checker.has_value()) {
            return {
                [](auto path) {
                    if (has_prefix(path, "/")) {
                        return path;
                    }
                    return concat_tostr(get_cwd(), path);
                }(package_loader->load_as_file(sf.checker.value(), "checker")),
                filename_to_lang(sf.checker.value())
            };
        }

        auto path = concat_tostr(tmp_dir.path(), "default_checker.c");
        put_file_contents(
            path, reinterpret_cast<const char*>(default_checker_c), default_checker_c_len
        );
        // Change the modification time to the compilation time to allow caching the default checker
        static timespec compilation_datetime = [] {
            constexpr auto compilation_datetime_str = noexcept_concat(__DATE__, ' ', __TIME__);
            struct tm t = {};
            throw_assert(
                strptime(compilation_datetime_str.c_str(), "%b %d %Y %H:%M:%S", &t) != nullptr
            );
            t.tm_isdst =
                -1; // use timezone information to check if daylight saving time is in effect
            time_t compilation_datetime = mktime(&t);
            throw_assert(compilation_datetime != static_cast<time_t>(-1));
            return timespec{.tv_sec = compilation_datetime, .tv_nsec = 0};
        }();
        timespec times[] = {{.tv_sec = 0, .tv_nsec = UTIME_OMIT}, compilation_datetime};
        if (utimensat(AT_FDCWD, path.c_str(), times, 0)) {
            THROW("utimensat()");
        }
        return {path, SolutionLanguage::C};
    }();

    checker_suite = lang_to_suite(lang);
    auto res = checker_suite->compile(
        source_path,
        {
            .time_limit = time_limit,
            .cpu_time_limit = time_limit,
            .memory_limit_in_bytes = compiler_memory_limit_in_bytes,
            .max_file_size_in_bytes = max_executable_size_in_bytes,
            .cache = cache ? std::optional{sim::judge::language_suite::Suite::CompileOptions::Cache{
                                 .compilation_cache = *cache,
                                 .cached_name = *cached_name, // NOLINT
                             }}
                           : std::nullopt,
        }
    );

    if (res.is_err()) {
        if (c_errors) {
            *c_errors = get_file_contents(std::move(res).unwrap_err(), 0, c_errors_max_len);
        }
        return 1;
    }
    return 0;
}

int JudgeWorker::compile_solution(
    FilePath source,
    SolutionLanguage lang,
    std::chrono::nanoseconds time_limit,
    uint64_t compiler_memory_limit_in_bytes,
    std::string* c_errors,
    size_t c_errors_max_len,
    judge::CompilationCache* cache,
    std::optional<std::string> cached_name
) {
    if (cache && !cached_name) {
        THROW("cached_name is required if cache is provided");
    }
    solution_suite = lang_to_suite(lang);
    auto res = solution_suite->compile(
        has_prefix(StringView{source}, "/") ? concat_tostr(source)
                                            : concat_tostr(get_cwd(), source),
        {
            .time_limit = time_limit,
            .cpu_time_limit = time_limit,
            .memory_limit_in_bytes = compiler_memory_limit_in_bytes,
            .max_file_size_in_bytes = max_executable_size_in_bytes,
            .cache = cache ? std::optional{sim::judge::language_suite::Suite::CompileOptions::Cache{
                                 .compilation_cache = *cache,
                                 .cached_name = *cached_name, // NOLINT
                             }}
                           : std::nullopt,
        }
    );

    if (res.is_err()) {
        if (c_errors) {
            *c_errors = get_file_contents(std::move(res).unwrap_err(), 0, c_errors_max_len);
        }
        return 1;
    }
    return 0;
}

void JudgeWorker::load_package(FilePath package_path, std::optional<string> simfile) {
    STACK_UNWINDING_MARK;

    if (is_directory(package_path)) {
        package_loader = std::make_unique<DirPackageLoader>(package_path);
    } else {
        package_loader = std::make_unique<ZipPackageLoader>(tmp_dir, package_path);
    }

    if (simfile.has_value()) {
        sf = Simfile(std::move(simfile.value()));
    } else {
        sf = Simfile(package_loader->load_as_str("Simfile"));
    }

    sf.load_tests_with_files();
    sf.load_checker();
}

// Real time limit is set to 1.5 * time_limit + 0.5s, because CPU time is
// measured
static inline auto cpu_time_limit_to_real_time_limit(std::chrono::nanoseconds cpu_tl) noexcept {
    return cpu_tl * 3 / 2 + std::chrono::milliseconds(500);
}

sandbox::result::Ok JudgeWorker::run_solution(
    FilePath input_file,
    FilePath output_file,
    std::chrono::nanoseconds time_limit,
    uint64_t memory_limit_in_bytes
) const {
    STACK_UNWINDING_MARK;

    using std::chrono_literals::operator""ns;

    if (time_limit <= 0ns) {
        THROW("If set, time_limit has to be greater than 0");
    }

    if (memory_limit_in_bytes <= 0) {
        THROW("If set, memory_limit_in_bytes has to be greater than 0");
    }

    FileDescriptor solution_stdout(output_file, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC);
    if (not solution_stdout.is_open()) {
        THROW("Failed to open file `", output_file, '`', errmsg());
    }
    FileDescriptor test_in(input_file, O_RDONLY | O_CLOEXEC);
    if (not test_in.is_open()) {
        THROW("Failed to open file `", input_file, '`', errmsg());
    }

    return solution_suite->await_result(solution_suite->async_run(
        {},
        {
            .stdin_fd = test_in,
            .stdout_fd = solution_stdout,
            .stderr_fd = std::nullopt,
            .time_limit = cpu_time_limit_to_real_time_limit(time_limit),
            .cpu_time_limit = time_limit,
            .memory_limit_in_bytes = memory_limit_in_bytes,
            .max_stack_size_in_bytes = memory_limit_in_bytes,
            .max_file_size_in_bytes = 1 << 30,
        },
        {}
    ));
}

namespace {

struct CheckerStatus {
    enum { OK, WRONG, ERROR } status = ERROR;

    double ratio = 0;
    std::string message;
};

} // namespace

template <class Func>
JudgeReport JudgeWorker::process_tests(
    bool final,
    JudgeLogger& judge_log,
    const std::optional<std::function<void(const JudgeReport&)>>& partial_report_callback,
    Func&& judge_on_test
) const {
    using std::chrono_literals::operator""s;

    JudgeReport report;
    judge_log.begin(final);

    // First round - judge as little as possible to compute total score
    bool test_were_skipped = false;
    uint64_t total_score = 0;
    uint64_t max_score = 0;
    for (const auto& group : sf.tgroups) {
        // Group "0" goes to the initial report, others groups to final
        auto p = Simfile::TestNameComparator::split(group.tests[0].name);
        if ((p.gid != "0") != final) {
            continue;
        }

        report.groups.emplace_back();
        auto& report_group = report.groups.back();

        double group_score_ratio = 1.0;
        auto calc_group_score = [&] {
            return static_cast<int64_t>(round(group.score * group_score_ratio));
        };

        bool skip_tests = false;
        for (const auto& test : group.tests) {
            if (skip_tests) {
                test_were_skipped = true;
                report_group.tests.emplace_back(
                    test.name,
                    JudgeReport::Test::SKIPPED,
                    0s,
                    test.time_limit,
                    0,
                    test.memory_limit,
                    string{}
                );
            } else {
                report_group.tests.emplace_back(judge_on_test(test, group_score_ratio));

                // Update group_score_ratio
                if (score_cut_lambda < 1) { // Only then the scaling occurs
                    const auto& test_report = report_group.tests.back();
                    const double x = std::chrono::duration<double>(test_report.runtime).count();
                    const double t = std::chrono::duration<double>(test_report.time_limit).count();
                    group_score_ratio =
                        std::min(group_score_ratio, (x / t - 1) / (score_cut_lambda - 1));
                    // Runtime may be greater than time_limit therefore
                    // group_score_ratio may become negative which is undesired
                    if (group_score_ratio < 0) {
                        group_score_ratio = 0;
                    }
                }

                if (group_score_ratio < 1e-6 and calc_group_score() == 0) {
                    skip_tests = partial_report_callback.has_value();
                }
            }
        }

        // Compute group score
        report_group.score = calc_group_score();
        report_group.max_score = group.score;

        total_score += report_group.score;
        if (report_group.max_score > 0) {
            max_score += report_group.max_score;
        }

        judge_log.group_score(report_group.score, report_group.max_score, group_score_ratio);
    }

    judge_log.final_score(total_score, max_score);

    if (test_were_skipped) {
        report.judge_log = judge_log.judge_log();
        partial_report_callback.value()(report);

        // Second round - judge remaining tests
        for (size_t gi = 0, rgi = 0; gi < sf.tgroups.size(); ++gi) {
            const auto& group = sf.tgroups[gi];

            // Group "0" goes to the initial report, others groups to final
            auto p = Simfile::TestNameComparator::split(group.tests[0].name);
            if ((p.gid != "0") != final) {
                continue;
            }

            auto& report_group = report.groups[rgi++];

            double group_score_ratio = 0; // Remaining tests have ratio == 0
            for (size_t ti = 0; ti < group.tests.size(); ++ti) {
                const auto& test = group.tests[ti];
                auto& test_report = report_group.tests[ti];
                if (test_report.status == JudgeReport::Test::SKIPPED) {
                    test_report = judge_on_test(test, group_score_ratio);
                }
            }
        }
    }

    judge_log.end();
    report.judge_log = judge_log.judge_log();
    return report;
}

JudgeReport JudgeWorker::judge(
    bool final,
    JudgeLogger& judge_log,
    const std::optional<std::function<void(const JudgeReport&)>>& partial_report_callback
) const {
    STACK_UNWINDING_MARK;
    auto judge_on_test = [&](const sim::Simfile::Test& test, double& group_score_ratio) {
        STACK_UNWINDING_MARK;

        auto tr = sf.interactive
            ? judge::test_on_interactive_test({
                  .compiled_program = *solution_suite,
                  .compiled_checker = *checker_suite,
                  .test_input = package_loader->load_as_file(test.in, "test.in"),
                  .program =
                      {
                          .time_limit = cpu_time_limit_to_real_time_limit(test.time_limit),
                          .cpu_time_limit = test.time_limit,
                          .memory_limit_in_bytes = test.memory_limit,
                      },
                  .checker =
                      {
                          .time_limit = cpu_time_limit_to_real_time_limit(checker_time_limit),
                          .cpu_time_limit = checker_time_limit,
                          .memory_limit_in_bytes = checker_memory_limit_in_bytes,
                          .max_comment_len = 256,
                      },
              })
            : judge::test_on_test({
                  .compiled_program = *solution_suite,
                  .compiled_checker = *checker_suite,
                  .test_input = package_loader->load_as_file(test.in, "test.in"),
                  .expected_output = package_loader->load_as_file(test.out.value(), "test.out"),
                  .program =
                      {
                          .time_limit = cpu_time_limit_to_real_time_limit(test.time_limit),
                          .cpu_time_limit = test.time_limit,
                          .memory_limit_in_bytes = test.memory_limit,
                          .output_size_limit_in_bytes = 1 << 30,
                      },
                  .checker =
                      {
                          .time_limit = cpu_time_limit_to_real_time_limit(checker_time_limit),
                          .cpu_time_limit = checker_time_limit,
                          .memory_limit_in_bytes = checker_memory_limit_in_bytes,
                          .max_comment_len = 256,
                      },
              });

        JudgeReport::Test test_report(
            test.name,
            JudgeReport::Test::OK,
            tr.program.cpu_time,
            test.time_limit,
            tr.program.peak_memory_in_bytes,
            test.memory_limit,
            string{}
        );

        group_score_ratio = std::min(group_score_ratio, tr.score);
        test_report.comment = tr.comment;

        switch (tr.status) {
        case judge::TestReport::Status::TimeLimitExceeded: {
            test_report.status = JudgeReport::Test::TLE;
            if (test_report.runtime < test_report.time_limit) {
                // Real time limit has been exceeded
                test_report.runtime = test_report.time_limit;
            }
        } break;
        case judge::TestReport::Status::MemoryLimitExceeded: {
            test_report.status = JudgeReport::Test::MLE;
        } break;
        case judge::TestReport::Status::OutputSizeLimitExceeded: {
            test_report.status = JudgeReport::Test::OLE;
        } break;
        case judge::TestReport::Status::RuntimeError: {
            test_report.status = JudgeReport::Test::RTE;
        } break;
        case judge::TestReport::Status::CheckerError: {
            test_report.status = JudgeReport::Test::CHECKER_ERROR;
        } break;
        case judge::TestReport::Status::WrongAnswer: {
            test_report.status = JudgeReport::Test::WA;
        } break;
        case judge::TestReport::Status::OK: {
            test_report.status = JudgeReport::Test::OK;
        } break;
        }

        // Logging
        judge_log.test(test.name, test_report, tr, checker_memory_limit_in_bytes);

        return test_report;
    };

    return process_tests(final, judge_log, partial_report_callback, judge_on_test);
}

} // namespace sim
