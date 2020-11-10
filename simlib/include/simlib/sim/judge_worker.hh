#pragma once

#include "simlib/debug.hh"
#include "simlib/file_manip.hh"
#include "simlib/file_path.hh"
#include "simlib/sandbox.hh"
#include "simlib/sim/compile.hh"
#include "simlib/sim/simfile.hh"
#include "simlib/temporary_directory.hh"
#include "simlib/time.hh"
#include "simlib/utilities.hh"

#include <utility>

namespace sim {

class JudgeReport {
public:
    struct Test {
        enum Status : uint8_t {
            OK,
            WA, // Wrong answer
            TLE, // Time limit exceeded
            MLE, // Memory limit exceeded
            RTE, // Runtime error
            CHECKER_ERROR,
            SKIPPED,
        };

        constexpr static CStringView description(Status st) {
            switch (st) {
            case OK: return CStringView{"OK"};
            case WA: return CStringView{"Wrong answer"};
            case TLE: return CStringView{"Time limit exceeded"};
            case MLE: return CStringView{"Memory limit exceeded"};
            case RTE: return CStringView{"Runtime error"};
            case CHECKER_ERROR: return CStringView{"Checker error"};
            case SKIPPED: return CStringView{"Skipped"};
            }

            // Should not happen but GCC complains about it
            return CStringView{"Unknown"};
        }

        std::string name;
        Status status;
        std::chrono::nanoseconds runtime, time_limit;
        uint64_t memory_consumed, memory_limit; // in bytes
        std::string comment;

        Test(
            std::string n, Status s, std::chrono::nanoseconds rt, std::chrono::nanoseconds tl,
            uint64_t mc, uint64_t ml, std::string c)
        : name(std::move(n))
        , status(s)
        , runtime(rt)
        , time_limit(tl)
        , memory_consumed(mc)
        , memory_limit(ml)
        , comment(std::move(c)) {}
    };

    struct Group {
        std::vector<Test> tests;
        int64_t score{}, max_score{}; // score belongs to [0, max_score], or if
                                      // max_score is negative, to [max_score, 0]
    };

    std::vector<Group> groups;
    std::string judge_log;

    /**
     * @brief Returns the pretty-printed judge report
     *
     * @param span_status Should take Test::Status as an argument and return
     *   something that will be appended to the dump as the test's status.
     *
     * @return A pretty-printed judge report
     */
    template <class Func>
    std::string pretty_dump(Func&& span_status) const {
        std::string res = "{\n";
        for (auto const& group : groups) {
            for (auto const& test : group.tests) {
                back_insert(
                    res, "  ", padded_string(test.name, 11, LEFT),
                    padded_string(
                        intentional_unsafe_string_view(
                            to_string(floor_to_10ms(test.runtime), false)),
                        4),
                    " / ", to_string(floor_to_10ms(test.time_limit), false), " s  ",
                    test.memory_consumed >> 10, " / ", test.memory_limit >> 10,
                    " KiB    Status: ");
                // Status
                res += span_status(test.status);

                // Comment
                if (!test.comment.empty()) {
                    back_insert(res, ' ', test.comment, '\n');
                } else {
                    res += '\n';
                }
            }

            // Score
            back_insert(res, "  Score: ", group.score, " / ", group.max_score, '\n');
        }

        return res += '}';
    }

    static const char* simple_span_status(Test::Status status) {
        switch (status) {
        case Test::OK: return "OK";
        case Test::WA: return "WA";
        case Test::TLE: return "TLE";
        case Test::MLE: return "MLE";
        case Test::RTE: return "RTE";
        case Test::CHECKER_ERROR: return "CHECKER_ERROR";
        case Test::SKIPPED: return "SKIPPED";
        }

        return "UNKNOWN";
    }

    /// Returns the pretty-printed judge report
    [[nodiscard]] std::string pretty_dump() const { return pretty_dump(&simple_span_status); }
};

enum class SolutionLanguage {
    UNKNOWN,
    C11,
    C = C11,
    CPP11,
    CPP14,
    CPP17,
    CPP = CPP17,
    PASCAL
};

inline bool is_source(StringView file) noexcept {
    return has_one_of_suffixes(file, ".c", ".cc", ".cpp", ".cxx", ".pas");
}

inline SolutionLanguage filename_to_lang(const StringView& filename) {
    SolutionLanguage res = SolutionLanguage::UNKNOWN;
    if (has_suffix(filename, ".c")) {
        res = SolutionLanguage::C;
    } else if (has_one_of_suffixes(filename, ".cc", ".cpp", ".cxx")) {
        res = SolutionLanguage::CPP;
    } else if (has_suffix(filename, ".pas")) {
        res = SolutionLanguage::PASCAL;
    }

    // It is written that way, so the compiler warns when the enum changes
    switch (res) {
    case SolutionLanguage::UNKNOWN:
    case SolutionLanguage::C:
    case SolutionLanguage::CPP:
    case SolutionLanguage::PASCAL:
        // If missing one, then update above ifs
        return res;
    case SolutionLanguage::CPP11:
    case SolutionLanguage::CPP14: break;
    }

    THROW("Should not reach here");
}

class JudgeLogger {
protected:
    std::string log_;

public:
    JudgeLogger() = default;

    JudgeLogger(const JudgeLogger&) = delete;
    JudgeLogger(JudgeLogger&&) = delete;
    JudgeLogger& operator=(const JudgeLogger&) = delete;
    JudgeLogger& operator=(JudgeLogger&&) = delete;

    virtual void begin(bool final) = 0;

    virtual void
    test(StringView test_name, JudgeReport::Test test_report, Sandbox::ExitStat es) = 0;

    virtual void test(
        StringView test_name, JudgeReport::Test test_report, Sandbox::ExitStat es,
        Sandbox::ExitStat checker_es, std::optional<uint64_t> checker_mem_limit,
        StringView checker_error_str) = 0;

    virtual void group_score(int64_t score, int64_t max_score, double score_ratio) = 0;

    virtual void final_score(int64_t score, int64_t max_score) = 0;

    virtual void end() = 0;

    [[nodiscard]] const std::string& judge_log() const noexcept { return log_; }

    virtual ~JudgeLogger() = default;
};

class VerboseJudgeLogger : public JudgeLogger {
    Logger dummy_logger_;
    Logger& logger_;

    bool after_final_score_{};
    bool first_test_after_final_score_{};
    InplaceBuff<8> last_gid;
    bool final_{};

    template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
    auto log(Args&&... args) {
        return DoubleAppender<decltype(log_)>(logger_, log_, std::forward<Args>(args)...);
    }

    template <class Func>
    void log_test(
        const StringView& test_name, const JudgeReport::Test& test_report,
        Sandbox::ExitStat es, Func&& func) {
        if (after_final_score_) {
            auto gid = sim::Simfile::TestNameComparator::split(test_name).gid;
            if (first_test_after_final_score_) {
                first_test_after_final_score_ = false;
            } else if (gid != last_gid) {
                log("");
            }

            last_gid = gid;
        }

        auto tmplog =
            log("  ", padded_string(test_name, 12, LEFT), ' ',
                padded_string(
                    intentional_unsafe_string_view(
                        to_string(floor_to_10ms(test_report.runtime), false)),
                    4),
                " / ", to_string(floor_to_10ms(test_report.time_limit), false), " s  ",
                test_report.memory_consumed >> 10, " / ", test_report.memory_limit >> 10,
                " KiB  Status: ");
        // Status
        switch (test_report.status) {
        case JudgeReport::Test::TLE: tmplog("\033[1;33mTLE\033[m"); break;
        case JudgeReport::Test::MLE: tmplog("\033[1;33mMLE\033[m"); break;
        case JudgeReport::Test::RTE: tmplog("\033[1;31mRTE\033[m (", es.message, ')'); break;
        case JudgeReport::Test::OK: tmplog("\033[1;32mOK\033[m"); break;
        case JudgeReport::Test::WA: tmplog("\033[1;31mWA\033[m"); break;
        case JudgeReport::Test::CHECKER_ERROR:
            tmplog("\033[1;35mCHECKER ERROR\033[m (Running \033[1;32mOK\033[m)");
            break;
        case JudgeReport::Test::SKIPPED: tmplog("\033[1;36mSKIPPED\033[m"); break;
        }

        // Rest
        tmplog(
            " [ CPU: ", to_string(es.cpu_runtime, false),
            " RT: ", to_string(es.runtime, false), " ]");

        func(tmplog);
    }

public:
    explicit VerboseJudgeLogger(bool log_to_stdlog = false)
    : dummy_logger_(nullptr)
    , logger_(log_to_stdlog ? stdlog : dummy_logger_) {}

    void begin(bool final) override {
        log_.clear();
        after_final_score_ = first_test_after_final_score_ = false;
        final_ = final;
        log("Judging (", (final ? "final" : "initial"), "): {");
    }

    void
    test(StringView test_name, JudgeReport::Test test_report, Sandbox::ExitStat es) override {
        log_test(test_name, test_report, es, [](auto& /*unused*/) {});
    }

    void test(
        StringView test_name, JudgeReport::Test test_report, Sandbox::ExitStat es,
        Sandbox::ExitStat checker_es, std::optional<uint64_t> checker_mem_limit,
        StringView checker_error_str) override {
        log_test(test_name, test_report, es, [&](auto& tmplog) {
            tmplog("  Checker: ");

            // Checker status
            if (test_report.status == JudgeReport::Test::OK) {
                tmplog("\033[1;32mOK\033[m ", test_report.comment);
            } else if (test_report.status == JudgeReport::Test::WA) {
                tmplog("\033[1;31mWA\033[m ", test_report.comment);
            } else if (test_report.status == JudgeReport::Test::CHECKER_ERROR) {
                tmplog("\033[1;35mERROR\033[m ", checker_error_str);
            } else {
                return; // Checker was not run
            }

            tmplog(
                " [ CPU: ", to_string(checker_es.cpu_runtime, false),
                " RT: ", to_string(checker_es.runtime, false), " ] ",
                checker_es.vm_peak >> 10);
            if (checker_mem_limit.has_value()) {
                tmplog(" / ", checker_mem_limit.value() >> 10, " KiB");
            }
        });
    }

    void group_score(int64_t score, int64_t max_score, double score_ratio) override {
        log("Score: ", score, " / ", max_score, " (ratio: ", to_string(score_ratio, 4), ')');
    }

    void final_score(int64_t total_score, int64_t max_score) override {
        after_final_score_ = first_test_after_final_score_ = true;
        if (final_) {
            log("Total score: ", total_score, " / ", max_score);
        }
    }

    void end() override { log('}'); }
};

class PackageLoader {
public:
    PackageLoader() = default;
    PackageLoader(const PackageLoader&) = default;
    PackageLoader(PackageLoader&&) = default;
    PackageLoader& operator=(const PackageLoader&) = default;
    PackageLoader& operator=(PackageLoader&&) = default;
    virtual ~PackageLoader() = default;

    /**
     * @brief Loads file with path @p path from package to file @p dest
     *
     * @param path path to file in package. If main dir == "foo/" and
     *   desired file has path "foo/bar/test", then @p path should be
     *   "bar/test"
     * @param dest path of the destination file
     *
     * @return @p dest is returned
     */
    virtual std::string load_into_dest_file(FilePath path, FilePath dest) = 0;

    /**
     * @brief Loads file with path @p path from package to a file
     *
     * @param path path to file in package. If main dir == "foo/" and
     *   desired file has path "foo/bar/test", then @p path should be
     *   "bar/test"
     * @param hint_name A proposition of the file name if a new file is created
     *
     * @return path to a loaded file
     */
    virtual std::string load_as_file(FilePath path, FilePath hint_name) = 0;

    /**
     * @brief Loads file with path @p path from package to a file
     *
     * @param path path to file in package. If main dir == "foo/" and
     *   desired file has path "foo/bar/test", then @p path should be
     *   "bar/test"
     *
     * @return contents of the loaded file
     */
    virtual std::string load_as_str(FilePath path) = 0;
};

/**
 * @brief Manages a judge worker
 * @details Only to use in ONE thread.
 */
class JudgeWorker {
    TemporaryDirectory tmp_dir{"/tmp/judge-worker.XXXXXX"};
    Simfile sf;

    static constexpr const char CHECKER_FILENAME[] = "checker";
    static constexpr const char SOLUTION_FILENAME[] = "solution";

    std::unique_ptr<PackageLoader> package_loader;

public:
    std::optional<std::chrono::nanoseconds> checker_time_limit = std::chrono::seconds(10);
    std::optional<uint64_t> checker_memory_limit = 256 << 20; // 256 MiB
    // It means that score ratio for runtime in range
    // [score_cut_lambda * time_limit, time_limit] falls linearly to 0.
    // For T = time limit, A = score_cut_lambda * T, the plot of score ratio
    // looks as follows:
    // +------------------------------------------+
    // |                                          |
    // |        ^ score ratio                     |
    // |        |                                 |
    // |      1 +--------------*                  |
    // |        |              |\                 |
    // |        |              | \                |
    // |        |              |  \               |
    // |      0 +--------------A---T---> time     |
    // |        0                                 |
    // +------------------------------------------+
    double score_cut_lambda = 2.0 / 3; // has to be from [0, 1]

    JudgeWorker() = default;

    JudgeWorker(const JudgeWorker&) = delete;
    JudgeWorker(JudgeWorker&&) noexcept = default;
    JudgeWorker& operator=(const JudgeWorker&) = delete;
    JudgeWorker& operator=(JudgeWorker&&) = default;

    ~JudgeWorker() = default;

    /// Loads package from @p package_path using @p simfile (if not specified,
    /// uses one found in the package)
    void load_package(FilePath package_path, std::optional<std::string> simfile);

    // Returns a reference to the loaded package's Simfile
    Simfile& simfile() noexcept { return sf; }

    // Returns a const reference to the loaded package's Simfile
    [[nodiscard]] const Simfile& simfile() const noexcept { return sf; }

private:
    int compile_impl(
        FilePath source, SolutionLanguage lang,
        std::optional<std::chrono::nanoseconds> time_limit, std::string* c_errors,
        size_t c_errors_max_len, const std::string& proot_path,
        StringView compilation_source_basename, StringView exec_dest_filename);

public:
    /// Compiles checker (using sim::compile())
    int compile_checker(
        std::optional<std::chrono::nanoseconds> time_limit, std::string* c_errors,
        size_t c_errors_max_len, const std::string& proot_path);

    /// Compiles solution (using sim::compile())
    int compile_solution(
        FilePath source, SolutionLanguage lang,
        std::optional<std::chrono::nanoseconds> time_limit, std::string* c_errors,
        size_t c_errors_max_len, const std::string& proot_path) {
        STACK_UNWINDING_MARK;
        return compile_impl(
            source, lang, time_limit, c_errors, c_errors_max_len, proot_path, "source",
            SOLUTION_FILENAME);
    }

    /// Compiles solution (using sim::compile())
    /// @p source should be a path in package main dir e.g. If main dir ==
    /// "foo/" and solution has path "foo/bar/test", then @p source should be
    /// "bar/test"
    int compile_solution_from_package(
        FilePath source, SolutionLanguage lang,
        std::optional<std::chrono::nanoseconds> time_limit, std::string* c_errors,
        size_t c_errors_max_len, const std::string& proot_path) {
        STACK_UNWINDING_MARK;
        auto solution_path = package_loader->load_as_file(source, "source");
        return compile_impl(
            solution_path, lang, time_limit, c_errors, c_errors_max_len, proot_path, "source",
            SOLUTION_FILENAME);
    }

    void load_compiled_checker(FilePath compiled_checker) {
        STACK_UNWINDING_MARK;
        thread_fork_safe_copy(
            compiled_checker, concat<PATH_MAX>(tmp_dir.path(), CHECKER_FILENAME), S_0755);
    }

    void load_compiled_solution(FilePath compiled_solution) {
        STACK_UNWINDING_MARK;
        thread_fork_safe_copy(
            compiled_solution, concat<PATH_MAX>(tmp_dir.path(), SOLUTION_FILENAME), S_0755);
    }

    void save_compiled_checker(
        FilePath destination, int (*copy_fn)(FilePath, FilePath, mode_t) = copy) const {
        STACK_UNWINDING_MARK;
        if (copy_fn(concat<PATH_MAX>(tmp_dir.path(), CHECKER_FILENAME), destination, S_0755)) {
            THROW("copy()", errmsg());
        }
    }

    void save_compiled_solution(
        FilePath destination, int (*copy_fn)(FilePath, FilePath, mode_t) = copy) const {
        STACK_UNWINDING_MARK;
        if (copy_fn(concat<PATH_MAX>(tmp_dir.path(), SOLUTION_FILENAME), destination, S_0755))
        {
            THROW("copy()", errmsg());
        }
    }

    /**
     * @brief Runs solution with @p input_file as stdin and @p output_file as
     *   stdout (this function makes no sense for the interactive problems)
     *
     * @param input_file path to file to set as stdin
     * @param output_file path to file to set as stdout
     * @param time_limit time limit
     * @param memory_limit memory limit in bytes
     * @return exit status of running the solution (in the sandbox)
     */
    [[nodiscard]] Sandbox::ExitStat run_solution(
        FilePath input_file, FilePath output_file,
        std::optional<std::chrono::nanoseconds> time_limit,
        std::optional<uint64_t> memory_limit) const;

private:
    template <class Func>
    JudgeReport process_tests(
        bool final, JudgeLogger& judge_log,
        const std::optional<std::function<void(const JudgeReport&)>>& partial_report_callback,
        Func&& judge_on_test) const;

    JudgeReport judge_interactive(
        bool final, JudgeLogger& judge_log,
        const std::optional<std::function<void(const JudgeReport&)>>& partial_report_callback =
            std::nullopt) const;

public:
    /**
     * @brief Judges last compiled solution on the last loaded package.
     * @details Before calling this method, the methods load_package(),
     *   compile_checker() and compile_solution() have to be called.
     *
     * @param final Whether judge only on final tests or only initial tests
     * @return Judge report
     */
    JudgeReport judge(
        bool final, JudgeLogger& judge_log,
        const std::optional<std::function<void(const JudgeReport&)>>& partial_report_callback =
            std::nullopt) const;

    [[nodiscard]] JudgeReport judge(bool final) const {
        VerboseJudgeLogger logger;
        return judge(final, logger);
    }
};

} // namespace sim
