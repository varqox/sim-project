#include "simlib/sim/conver.hh"
#include "simlib/concat.hh"
#include "simlib/concat_tostr.hh"
#include "simlib/concurrent/job_processor.hh"
#include "simlib/config_file.hh"
#include "simlib/debug.hh"
#include "simlib/directory.hh"
#include "simlib/file_info.hh"
#include "simlib/file_path.hh"
#include "simlib/inplace_buff.hh"
#include "simlib/libzip.hh"
#include "simlib/opened_temporary_file.hh"
#include "simlib/path.hh"
#include "simlib/process.hh"
#include "simlib/sim/judge_worker.hh"
#include "simlib/string_compare.hh"
#include "simlib/temporary_file.hh"
#include "simlib/time.hh"
#include "test/compilation_cache.hh"

#include <chrono>
#include <gtest/gtest.h>
#include <linux/limits.h>
#include <regex>
#include <utility>

using sim::Conver;
using sim::JudgeReport;
using sim::JudgeWorker;
using std::optional;
using std::pair; // NOLINT(misc-unused-using-decls)
using std::string;

constexpr static bool regenerate_outs = false;
constexpr auto test_cases_subdir = "tests/";
constexpr auto packages_subdir = "packages/";

namespace test_filenames {
constexpr auto config = "config";
constexpr auto pre_judge_simfile = "pre_judge_simfile";
constexpr auto judge_log = "judge_log";
constexpr auto post_judge_simfile = "post_judge_simfile";
constexpr auto conver_report = "conver_report";
} // namespace test_filenames

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

    auto get_string = [&](StringView name) -> const string& {
        return get_var(name).as_string();
    };

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

    auto get_optional_duration =
        [&](const StringView& name) -> optional<std::chrono::nanoseconds> {
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
    conf.override_main_solution_with =
        get_optional_string("override_main_solution_with", false);

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
    conf.opts.rtl_opts.solution_runtime_coefficient =
        get_double("solution_runtime_coefficient");

    return conf;
}

class TestingJudgeLogger : public sim::JudgeLogger {
    template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
    void log(Args&&... args) {
        back_insert(log_, std::forward<Args>(args)...);
    }

    template <class Func>
    void log_test(
        StringView test_name, const JudgeReport::Test& test_report,
        const Sandbox::ExitStat& es, Func&& func) {
        log("  ", padded_string(test_name, 8, LEFT), ' ',
            " [ TL: ", to_string(floor_to_10ms(test_report.time_limit), false),
            " s ML: ", test_report.memory_limit >> 10,
            " KiB ]  Status: ", JudgeReport::simple_span_status(test_report.status));

        if (test_report.status == JudgeReport::Test::RTE) {
            log(" (",
                std::regex_replace(es.message, std::regex("killed and dumped"), "killed"),
                ')');
        }
        func();
        log('\n');
    }

public:
    TestingJudgeLogger() = default;

    void begin(bool final) override {
        log_.clear();
        log("Judging (", (final ? "final" : "initial"), "): {\n");
    }

    void
    test(StringView test_name, JudgeReport::Test test_report, Sandbox::ExitStat es) override {
        log_test(test_name, test_report, es, [] {});
    }

    void test(
        StringView test_name, JudgeReport::Test test_report, Sandbox::ExitStat es,
        Sandbox::ExitStat /*checker_es*/, optional<uint64_t> checker_mem_limit,
        StringView checker_error_str) override {
        log_test(test_name, test_report, es, [&]() {
            log("  Checker: ");

            // Checker status
            if (test_report.status == JudgeReport::Test::OK) {
                log("OK: ", test_report.comment);
            } else if (test_report.status == JudgeReport::Test::WA) {
                log("WA: ", test_report.comment);
            } else if (test_report.status == JudgeReport::Test::CHECKER_ERROR) {
                log("ERROR: ", checker_error_str);
            } else {
                return; // Checker was not run
            }

            if (checker_mem_limit.has_value()) {
                log(" [ ML: ", checker_mem_limit.value() >> 10, " KiB ]");
            }
        });
    }

    void group_score(int64_t score, int64_t max_score, double score_ratio) override {
        log("Score: ", score, " / ", max_score, " (ratio: ", to_string(score_ratio, 4), ")\n");
    }

    void final_score(int64_t /*score*/, int64_t /*max_score*/) override {}

    void end() override { log("}\n"); }
};

class ConverTestCaseRunner {
    static constexpr std::chrono::seconds COMPILATION_TIME_LIMIT{5};
    static constexpr size_t COMPILATION_ERRORS_MAX_LENGTH = 4096;

    const string& tests_dir_;
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
    : tests_dir_(tests_dir)
    , test_case_name_(std::move(test_case_name))
    , test_case_dir_(concat(tests_dir, test_cases_subdir, test_case_name_, '/'))
    , conf_(load_config_from_file(concat(test_case_dir_, test_filenames::config))) {
        conver_.package_path(concat_tostr(tests_dir, packages_subdir, conf_.pkg_path));
    }

    void run() {
        try {
            generate_result();
        } catch (const std::exception& e) {
            conver_report_ = concat_tostr(
                conver_.report(), "\n>>>> Exception caught <<<<\n",
                std::regex_replace(
                    e.what(), std::regex(R"=(\(thrown at (\w|\.|/)+:\d+\))="),
                    "(thrown at ...)"));
        }

        check_result();
    }

private:
    void generate_result() {
        auto cres = conver_.construct_simfile(conf_.opts);
        if (conf_.override_main_solution_with) {
            auto const& new_sol = *conf_.override_main_solution_with;
            auto& solutions = cres.simfile.solutions;
            solutions.erase(
                std::remove(solutions.begin(), solutions.end(), new_sol), solutions.end());
            solutions.emplace(solutions.begin(), new_sol);
        }
        pre_judge_simfile_ = cres.simfile;
        post_judge_simfile_ = cres.simfile;
        conver_report_ = conver_.report();

        switch (cres.status) {
        case Conver::Status::COMPLETE: return; // Nothing more to do
        case Conver::Status::NEED_MODEL_SOLUTION_JUDGE_REPORT: break;
        }

        JudgeWorker jworker;
        jworker.load_package(conver_.package_path(), post_judge_simfile_.dump());
        compile_checker_and_solution(jworker);

        TestingJudgeLogger judge_logger;
        initial_judge_report_ = jworker.judge(false, judge_logger),
        final_judge_report_ = jworker.judge(true, judge_logger);

        Conver::reset_time_limits_using_jugde_reports(
            post_judge_simfile_, initial_judge_report_, final_judge_report_,
            conf_.opts.rtl_opts);
    }

    void compile_checker_and_solution(JudgeWorker& jworker) {
        using time_point = std::chrono::system_clock::time_point;
        CompilationCache ccache = {
            "/tmp/simlib-conver-test-compilation-cache/", std::chrono::hours(24)};
        string compilation_errors;
        // Checker
        auto [checker_cache_path, checker_mtime] = [&] {
            string path;
            time_point tp;
            if (not pre_judge_simfile_.checker) {
                path = "default_checker";
                tp = get_modification_time(
                    concat(tests_dir_, "../../src/sim/default_checker.c"));
            } else if (is_directory(conver_.package_path())) {
                path = concat_tostr(conver_.package_path(), *pre_judge_simfile_.checker);
                tp = get_modification_time(path);
            } else {
                path = concat_tostr(conver_.package_path(), '/', *pre_judge_simfile_.checker);
                tp = get_modification_time(conver_.package_path());
            }
            return pair{path, tp};
        }();

        if (ccache.is_cached(checker_cache_path, checker_mtime)) {
            jworker.load_compiled_checker(ccache.cached_path(checker_cache_path));
        } else {
            if (jworker.compile_checker(
                    COMPILATION_TIME_LIMIT, &compilation_errors, COMPILATION_ERRORS_MAX_LENGTH,
                    ""))
            {
                THROW("failed to compile checker: \n", compilation_errors);
            }
            ccache.cache_compiled_checker(checker_cache_path, jworker);
        }

        // Solution
        auto const& main_solution = post_judge_simfile_.solutions[0];
        auto [sol_cache_path, sol_mtime] = [&] {
            string path;
            time_point tp;
            if (is_directory(conver_.package_path())) {
                path = concat_tostr(conver_.package_path(), main_solution);
                tp = get_modification_time(path);
            } else {
                path = concat_tostr(conver_.package_path(), '/', main_solution);
                tp = get_modification_time(conver_.package_path());
            }
            return pair{path, tp};
        }();

        if (ccache.is_cached(sol_cache_path, sol_mtime)) {
            jworker.load_compiled_solution(ccache.cached_path(sol_cache_path));
        } else {
            if (jworker.compile_solution_from_package(
                    main_solution, sim::filename_to_lang(main_solution),
                    COMPILATION_TIME_LIMIT, &compilation_errors, COMPILATION_ERRORS_MAX_LENGTH,
                    ""))
            {
                THROW("failed to compile solution: \n", compilation_errors);
            }
            ccache.cache_compiled_solution(sol_cache_path, jworker);
        }
    }

    void round_post_judge_simfile_time_limits_to_whole_seconds() {
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
        round_post_judge_simfile_time_limits_to_whole_seconds();
        if (regenerate_outs) {
            overwrite_test_output_files();
        }

        EXPECT_EQ(
            get_file_contents(concat_tostr(test_case_dir_, test_filenames::pre_judge_simfile)),
            pre_judge_simfile_.dump())
            << "^ test " << test_case_name_;
        EXPECT_EQ(
            get_file_contents(
                concat_tostr(test_case_dir_, test_filenames::post_judge_simfile)),
            post_judge_simfile_.dump())
            << "^ test " << test_case_name_;
        EXPECT_EQ(
            get_file_contents(concat_tostr(test_case_dir_, test_filenames::conver_report)),
            conver_report_)
            << "^ test " << test_case_name_;
        EXPECT_EQ(
            get_file_contents(concat_tostr(test_case_dir_, test_filenames::judge_log)),
            serialized_judge_log())
            << "^ test " << test_case_name_;
    }

    [[nodiscard]] string serialized_judge_log() const {
        return initial_judge_report_.judge_log + final_judge_report_.judge_log;
    }

    void overwrite_test_output_files() const {
        put_file_contents(
            concat_tostr(test_case_dir_, test_filenames::pre_judge_simfile),
            intentional_unsafe_string_view(pre_judge_simfile_.dump()));

        put_file_contents(
            concat_tostr(test_case_dir_, test_filenames::post_judge_simfile),
            intentional_unsafe_string_view(post_judge_simfile_.dump()));

        put_file_contents(
            concat_tostr(test_case_dir_, test_filenames::conver_report), conver_report_);

        put_file_contents(
            concat_tostr(test_case_dir_, test_filenames::judge_log),
            intentional_unsafe_string_view(serialized_judge_log()));
    }
};

class ConverTestsRunner : public concurrent::JobProcessor<string> {
    const string tests_dir_;

public:
    explicit ConverTestsRunner(string tests_dir)
    : tests_dir_(std::move(tests_dir)) {}

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

// NOLINTNEXTLINE
TEST(Conver, construct_simfile) {
    stdlog.label(false);
    stdlog.use(stdout);

    for (const auto& path : {string{"."}, executable_path(getpid())}) {
        auto tests_dir_opt =
            deepest_ancestor_dir_with_subpath(path, "test/conver_test_cases/");
        if (tests_dir_opt) {
            ConverTestsRunner(*tests_dir_opt).run();
            return;
        }
    }

    FAIL() << "could not find tests directory";
}
