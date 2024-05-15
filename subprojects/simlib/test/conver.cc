#include <cassert>
#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>
#include <linux/limits.h>
#include <regex>
#include <simlib/concat.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/concurrent/job_processor.hh>
#include <simlib/config_file.hh>
#include <simlib/directory.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_info.hh>
#include <simlib/file_path.hh>
#include <simlib/inplace_buff.hh>
#include <simlib/logger.hh>
#include <simlib/path.hh>
#include <simlib/sim/conver.hh>
#include <simlib/sim/judge/disk_compilation_cache.hh>
#include <simlib/sim/judge/test_report.hh>
#include <simlib/sim/judge_worker.hh>
#include <simlib/string_compare.hh>
#include <simlib/string_view.hh>
#include <simlib/time_format_conversions.hh>
#include <utility>

using sim::Conver;
using sim::JudgeReport;
using sim::JudgeWorker;
using std::optional;
using std::pair; // NOLINT(misc-unused-using-decls)
using std::string;

static constexpr bool regenerate_outs = false;
constexpr auto test_cases_subdir = "tests/";
constexpr auto packages_subdir = "packages/";

namespace test_filenames {
constexpr auto config = "config";
constexpr auto pre_judge_simfile = "pre_judge_simfile";
constexpr auto judge_log = "judge_log";
constexpr auto post_judge_simfile = "post_judge_simfile";
constexpr auto conver_report = "conver_report";
} // namespace test_filenames

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
CStringView compilation_cache_dir;

struct TestConfig {
    string pkg_path;
    optional<string> override_main_solution_with;
    Conver::Options opts;
};

static TestConfig load_config_from_file(FilePath file) {
    ConfigFile cf;
    cf.load_config_from_file(file, true);

    auto get_var = [&](StringView name, bool error_if_unset = true) -> decltype(auto) {
        auto const& var = cf[name];
        if (not var.is_set()) {
            if (error_if_unset) {
                THROW("Variable \"", name, "\" is not set");
            }
            return var;
        }
        if (var.is_array()) {
            THROW("Variable \"", name, "\" is an array");
        }
        return var;
    };

    auto get_string = [&](StringView name) -> const string& { return get_var(name).as_string(); };

    auto get_optional_string = [&](const StringView& name,
                                   bool error_if_unset = true) -> optional<string> {
        auto const& var = get_var(name, error_if_unset);
        if (not var.is_set() or var.as_string() == "null") {
            return std::nullopt;
        }

        return get_string(name);
    };

    auto get_uint64 = [&](StringView name) { return get_var(name).as<uint64_t>().value(); };

    auto get_optional_uint64 = [&](const StringView& name) -> optional<uint64_t> {
        if (get_var(name).as_string() == "null") {
            return std::nullopt;
        }

        return get_uint64(name);
    };

    auto get_double = [&](StringView name) { return get_var(name).as<double>().value(); };

    auto get_duration = [&](StringView name) {
        using std::chrono::nanoseconds;
        using std::chrono::duration;
        using std::chrono::duration_cast;
        return duration_cast<nanoseconds>(duration<double>(get_double(name)));
    };

    auto get_optional_duration = [&](const StringView& name) -> optional<std::chrono::nanoseconds> {
        if (get_var(name).as_string() == "null") {
            return std::nullopt;
        }

        return get_duration(name);
    };

    auto get_bool = [&](StringView name) {
        StringView str = get_string(name);
        if (str == "true") {
            return true;
        }
        if (str == "false") {
            return false;
        }
        THROW("variable \"", name, "\" is not a bool: ", str);
    };

    auto get_optional_bool = [&](const StringView& name) -> optional<bool> {
        if (get_var(name).as_string() == "null") {
            return std::nullopt;
        }

        return get_bool(name);
    };

    TestConfig conf;
    conf.pkg_path = get_string("package");
    conf.override_main_solution_with = get_optional_string("override_main_solution_with", false);

    conf.opts.name = get_optional_string("name");
    conf.opts.label = get_optional_string("label");
    conf.opts.interactive = get_optional_bool("interactive");
    conf.opts.memory_limit = get_optional_uint64("memory_limit");
    conf.opts.global_time_limit = get_optional_duration("global_time_limit");
    conf.opts.max_time_limit = get_duration("max_time_limit");
    conf.opts.reset_time_limits_using_main_solution =
        get_bool("reset_time_limits_using_main_solution");
    conf.opts.ignore_simfile = get_bool("ignore_simfile");
    conf.opts.seek_for_new_tests = get_bool("seek_for_new_tests");
    conf.opts.reset_scoring = get_bool("reset_scoring");
    conf.opts.require_statement = get_bool("require_statement");
    conf.opts.rtl_opts.min_time_limit = get_duration("min_time_limit");
    conf.opts.rtl_opts.solution_runtime_coefficient = get_double("solution_runtime_coefficient");

    return conf;
}

class TestingJudgeLogger : public sim::JudgeLogger {
    template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
    void log(Args&&... args) {
        back_insert(log_, std::forward<Args>(args)...);
    }

public:
    TestingJudgeLogger() = default;

    void begin(bool final) override {
        log_.clear();
        log("Judging (", (final ? "final" : "initial"), "): {\n");
    }

    void test(
        StringView test_name,
        JudgeReport::Test test_report,
        sim::judge::TestReport judge_test_report,
        uint64_t checker_mem_limit
    ) override {
        log("  ",
            padded_string(test_name, 8, LEFT),
            ' ',
            " [ TL: ",
            to_string(floor_to_10ms(test_report.time_limit), false),
            " s ML: ",
            test_report.memory_limit >> 10,
            " KiB ]  Status: ",
            JudgeReport::simple_span_status(test_report.status));

        if (!test_report.comment.empty()) {
            auto normalized_comment = test_report.comment;
            constexpr std::string_view STR_TO_REPLACE = " killed and dumped by signal ";
            auto pos = normalized_comment.find(STR_TO_REPLACE);
            if (pos != decltype(normalized_comment)::npos) {
                normalized_comment.replace(pos, STR_TO_REPLACE.size(), " killed by signal ");
            }
            log(" (", normalized_comment, ")");
        }
        if (judge_test_report.checker) {
            log("  Checker: ");

            // Checker status
            if (checker_mem_limit) {
                log(" [ ML: ", checker_mem_limit >> 10, " KiB ]");
            }
        }
        log('\n');
    }

    void group_score(int64_t score, int64_t max_score, double score_ratio) override {
        log("Score: ", score, " / ", max_score, " (ratio: ", to_string(score_ratio, 4), ")\n");
    }

    void final_score(int64_t /*score*/, int64_t /*max_score*/) override {}

    void end() override { log("}\n"); }
};

class ConverTestCaseRunner {
    static constexpr std::chrono::seconds COMPILATION_TIME_LIMIT{30};
    static constexpr uint64_t COMPILATION_MEMORY_LIMIT = 1 << 30;
    static constexpr size_t COMPILATION_ERRORS_MAX_LENGTH = 4096;

    const string test_case_name_;
    const InplaceBuff<PATH_MAX> test_case_dir_;
    const TestConfig conf_;

    Conver conver_;
    string conver_report_;
    sim::Simfile pre_judge_simfile_;
    sim::Simfile post_judge_simfile_;
    JudgeReport initial_judge_report_, final_judge_report_;

public:
    ConverTestCaseRunner(const string& tests_dir, string&& test_case_name)
    : test_case_name_(std::move(test_case_name))
    , test_case_dir_(concat(tests_dir, test_cases_subdir, test_case_name_, '/'))
    , conf_(load_config_from_file(concat(test_case_dir_, test_filenames::config))) {
        conver_.package_path(concat_tostr(tests_dir, packages_subdir, conf_.pkg_path));
    }

    void run() {
        try {
            generate_result();
        } catch (const std::exception& e) {
            conver_report_ = concat_tostr(
                conver_.report(),
                "\n>>>> Exception caught <<<<\n",
                std::regex_replace(
                    e.what(), std::regex(R"=(\(thrown at (\w|-|\.|/)+:\d+\))="), "(thrown at ...)"
                )
            );
        }

        check_result();
    }

private:
    void generate_result() {
        auto cres = conver_.construct_simfile(conf_.opts);
        if (conf_.override_main_solution_with) {
            const auto& new_sol = *conf_.override_main_solution_with;
            auto& solutions = cres.simfile.solutions;
            solutions.erase(
                std::remove(solutions.begin(), solutions.end(), new_sol), solutions.end()
            );
            solutions.emplace(solutions.begin(), new_sol);
        }
        pre_judge_simfile_ = cres.simfile;
        post_judge_simfile_ = cres.simfile;
        conver_report_ = conver_.report();

        switch (cres.status) {
        case Conver::Status::COMPLETE: return; // Nothing more to do
        case Conver::Status::NEED_MODEL_SOLUTION_JUDGE_REPORT: break;
        }

        auto jworker = JudgeWorker{{
            .checker_memory_limit_in_bytes = 256 << 20,
        }};
        jworker.load_package(conver_.package_path(), post_judge_simfile_.dump());
        compile_checker_and_solution(jworker);

        TestingJudgeLogger judge_logger;
        initial_judge_report_ = jworker.judge(false, judge_logger),
        final_judge_report_ = jworker.judge(true, judge_logger);

        Conver::reset_time_limits_using_jugde_reports(
            post_judge_simfile_, initial_judge_report_, final_judge_report_, conf_.opts.rtl_opts
        );
    }

    void compile_checker_and_solution(JudgeWorker& jworker) {
        auto compilation_cache = sim::judge::DiskCompilationCache{
            compilation_cache_dir.to_string(), std::chrono::hours(7 * 24)
        };
        string compilation_errors;
        if (jworker.compile_checker(
                COMPILATION_TIME_LIMIT,
                COMPILATION_MEMORY_LIMIT,
                &compilation_errors,
                COMPILATION_ERRORS_MAX_LENGTH,
                &compilation_cache,
                concat_tostr(conver_.package_path(), ":checker")
            ))
        {
            THROW("failed to compile checker: \n", compilation_errors);
        }

        // Solution
        const auto& main_solution = post_judge_simfile_.solutions[0];
        if (jworker.compile_solution_from_package(
                main_solution,
                sim::filename_to_lang(main_solution),
                COMPILATION_TIME_LIMIT,
                COMPILATION_MEMORY_LIMIT,
                &compilation_errors,
                COMPILATION_ERRORS_MAX_LENGTH,
                &compilation_cache,
                concat_tostr(conver_.package_path(), ':', main_solution)
            ))
        {
            THROW("failed to compile solution: \n", compilation_errors);
        }
    }

    void round_post_judge_simfile_time_limits_to_multiple_of_one_seconds() {
        using std::chrono_literals::operator""s;
        // This should remove the problem with random time limit if they
        // were set using the model solution.
        for (auto& group : post_judge_simfile_.tgroups) {
            for (auto& test : group.tests) {
                // Time limits should not have been set to 0
                EXPECT_GT(test.time_limit, 0s) << "^ test " << test_case_name_;
                test.time_limit =
                    std::chrono::duration_cast<std::chrono::seconds>(test.time_limit + 0.5s);
            }
        }
    }

    void check_result() {
        round_post_judge_simfile_time_limits_to_multiple_of_one_seconds();
        if (regenerate_outs) {
            overwrite_test_output_files();
        }

        EXPECT_EQ(
            get_file_contents(concat_tostr(test_case_dir_, test_filenames::pre_judge_simfile)),
            pre_judge_simfile_.dump()
        ) << "^ test "
          << test_case_name_;
        EXPECT_EQ(
            get_file_contents(concat_tostr(test_case_dir_, test_filenames::post_judge_simfile)),
            post_judge_simfile_.dump()
        ) << "^ test "
          << test_case_name_;
        EXPECT_EQ(
            get_file_contents(concat_tostr(test_case_dir_, test_filenames::conver_report)),
            conver_report_
        ) << "^ test "
          << test_case_name_;
        EXPECT_EQ(
            get_file_contents(concat_tostr(test_case_dir_, test_filenames::judge_log)),
            serialized_judge_log()
        ) << "^ test "
          << test_case_name_;
    }

    [[nodiscard]] string serialized_judge_log() const {
        return initial_judge_report_.judge_log + final_judge_report_.judge_log;
    }

    void overwrite_test_output_files() const {
        put_file_contents(
            concat_tostr(test_case_dir_, test_filenames::pre_judge_simfile),
            from_unsafe{pre_judge_simfile_.dump()}
        );

        put_file_contents(
            concat_tostr(test_case_dir_, test_filenames::post_judge_simfile),
            from_unsafe{post_judge_simfile_.dump()}
        );

        put_file_contents(
            concat_tostr(test_case_dir_, test_filenames::conver_report), conver_report_
        );

        put_file_contents(
            concat_tostr(test_case_dir_, test_filenames::judge_log),
            from_unsafe{serialized_judge_log()}
        );
    }
};

class ConverTestsRunner : public concurrent::JobProcessor<string> {
    const string tests_dir_;

public:
    explicit ConverTestsRunner(string tests_dir) : tests_dir_(std::move(tests_dir)) {}

protected:
    void produce_jobs() final {
        std::vector<string> test_cases;
        // Collect available test cases
        for_each_dir_component(concat(tests_dir_, test_cases_subdir), [&](dirent* file) {
            test_cases.emplace_back(file->d_name);
        });
        sort(test_cases.begin(), test_cases.end(), StrVersionCompare{});
        reverse(test_cases.begin(), test_cases.end());
        for (auto& test_case_name : test_cases) {
            add_job(std::move(test_case_name));
        }
    }

    void process_job(string test_case_name) final {
        try {
            stdlog("Running test case: ", test_case_name);
            ConverTestCaseRunner(tests_dir_, std::move(test_case_name)).run();
        } catch (const std::exception& e) {
            FAIL() << "Unexpected exception -> " << e.what();
        }
    }
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
CStringView test_cases_dir;

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    assert(argc == 3);
    test_cases_dir = CStringView{argv[1]};
    compilation_cache_dir = CStringView{argv[2]};

    return RUN_ALL_TESTS();
}

// NOLINTNEXTLINE
TEST(Conver, run) {
    stdlog.label(false);
    ConverTestsRunner(test_cases_dir.to_string()).run();
}
