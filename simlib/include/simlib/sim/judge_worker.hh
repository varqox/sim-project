#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <simlib/concat.hh>
#include <simlib/file_manip.hh>
#include <simlib/file_path.hh>
#include <simlib/logger.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/sim/judge/compilation_cache.hh>
#include <simlib/sim/judge/language_suite/suite.hh>
#include <simlib/sim/judge/test_report.hh>
#include <simlib/sim/simfile.hh>
#include <simlib/temporary_directory.hh>
#include <simlib/time_format_conversions.hh>
#include <simlib/to_string.hh>
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
            OLE, // Output size limit exceeded
            RTE, // Runtime error
            CHECKER_ERROR,
            SKIPPED,
        };

        static constexpr CStringView description(Status st) {
            switch (st) {
            case OK: return CStringView{"OK"};
            case WA: return CStringView{"Wrong answer"};
            case TLE: return CStringView{"Time limit exceeded"};
            case MLE: return CStringView{"Memory limit exceeded"};
            case OLE: return CStringView{"Output size limit exceeded"};
            case RTE: return CStringView{"Runtime error"};
            case CHECKER_ERROR: return CStringView{"Checker error"};
            case SKIPPED: return CStringView{"Skipped"};
            }

            // Should not happen but GCC complains about it
            return CStringView{"Unknown"};
        }

        std::string name;
        Status status;
        std::chrono::nanoseconds runtime;
        std::chrono::nanoseconds time_limit;
        uint64_t memory_consumed; // in bytes
        uint64_t memory_limit; // in bytes
        std::string comment;

        Test(
            std::string n,
            Status s,
            std::chrono::nanoseconds rt,
            std::chrono::nanoseconds tl,
            uint64_t mc,
            uint64_t ml,
            std::string c
        )
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
        for (const auto& group : groups) {
            for (const auto& test : group.tests) {
                back_insert(
                    res,
                    "  ",
                    padded_string(test.name, 11, LEFT),
                    padded_string(from_unsafe{::to_string(floor_to_10ms(test.runtime), false)}, 4),
                    " / ",
                    ::to_string(floor_to_10ms(test.time_limit), false),
                    " s  ",
                    test.memory_consumed >> 10,
                    " / ",
                    test.memory_limit >> 10,
                    " KiB    Status: "
                );
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
        case Test::OLE: return "OLE";
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
    PASCAL,
    PYTHON,
    RUST,
};

inline bool is_source(StringView file) noexcept {
    return has_one_of_suffixes(file, ".c", ".cc", ".cpp", ".cxx", ".pas", ".py", ".rs");
}

inline SolutionLanguage filename_to_lang(const StringView& filename) {
    SolutionLanguage res = SolutionLanguage::UNKNOWN;
    if (has_suffix(filename, ".c")) {
        res = SolutionLanguage::C;
    } else if (has_one_of_suffixes(filename, ".cc", ".cpp", ".cxx")) {
        res = SolutionLanguage::CPP;
    } else if (has_suffix(filename, ".pas")) {
        res = SolutionLanguage::PASCAL;
    } else if (has_suffix(filename, ".py")) {
        res = SolutionLanguage::PYTHON;
    } else if (has_suffix(filename, ".rs")) {
        res = SolutionLanguage::RUST;
    }

    // It is written that way, so the compiler warns when the enum changes
    switch (res) {
    case SolutionLanguage::UNKNOWN:
    case SolutionLanguage::C:
    case SolutionLanguage::CPP:
    case SolutionLanguage::PASCAL:
    case SolutionLanguage::PYTHON:
    case SolutionLanguage::RUST:
        // If missing one, then update above ifs
        return res;
    case SolutionLanguage::CPP11:
    case SolutionLanguage::CPP14: break;
    }

    THROW("Should not reach here");
}

std::unique_ptr<judge::language_suite::Suite> lang_to_suite(SolutionLanguage lang);

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

    virtual void test(
        StringView test_name,
        JudgeReport::Test test_report,
        judge::TestReport judge_test_report,
        uint64_t checker_mem_limit
    ) = 0;

    virtual void group_score(int64_t score, int64_t max_score, double score_ratio) = 0;

    virtual void final_score(int64_t score, int64_t max_score) = 0;

    virtual void end() = 0;

    [[nodiscard]] const std::string& judge_log() const noexcept { return log_; }

    virtual ~JudgeLogger() = default;
};

class VerboseJudgeLogger : public JudgeLogger {
    Logger dummy_logger_;
    Logger& logger_; // NOLINT

    bool after_final_score_{};
    bool first_test_after_final_score_{};
    InplaceBuff<8> last_gid;
    bool final_{};

    template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
    auto log(Args&&... args) {
        return DoubleAppender<decltype(log_)>(logger_, log_, std::forward<Args>(args)...);
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

    void test(
        StringView test_name,
        JudgeReport::Test test_report,
        judge::TestReport judge_test_report,
        uint64_t checker_mem_limit
    ) override {
        if (after_final_score_) {
            auto gid = sim::Simfile::TestNameComparator::split(test_name).gid;
            if (first_test_after_final_score_) {
                first_test_after_final_score_ = false;
            } else if (gid != last_gid) {
                log("");
            }

            last_gid = gid;
        }

        auto tmplog = log(
            "  ",
            padded_string(test_name, 12, LEFT),
            ' ',
            padded_string(from_unsafe{::to_string(floor_to_10ms(test_report.runtime), false)}, 4),
            " / ",
            ::to_string(floor_to_10ms(test_report.time_limit), false),
            " s  ",
            test_report.memory_consumed >> 10,
            " / ",
            test_report.memory_limit >> 10,
            " KiB  Status: "
        );
        // Status
        switch (test_report.status) {
        case JudgeReport::Test::TLE: tmplog("\033[1;33mTLE\033[m"); break;
        case JudgeReport::Test::MLE: tmplog("\033[1;33mMLE\033[m"); break;
        case JudgeReport::Test::OLE: tmplog("\033[1;33mOLE\033[m"); break;
        case JudgeReport::Test::RTE: tmplog("\033[1;31mRTE\033[m"); break;
        case JudgeReport::Test::OK: tmplog("\033[1;32mOK\033[m"); break;
        case JudgeReport::Test::WA: tmplog("\033[1;31mWA\033[m"); break;
        case JudgeReport::Test::CHECKER_ERROR: tmplog("\033[1;35mCHECKER ERROR\033[m"); break;
        case JudgeReport::Test::SKIPPED: tmplog("\033[1;36mSKIPPED\033[m"); break;
        }
        if (!test_report.comment.empty()) {
            tmplog(" (", test_report.comment, ')');
        }

        // Rest
        tmplog(
            " [ CPU: ",
            ::to_string(judge_test_report.program.cpu_time, false),
            " RT: ",
            ::to_string(judge_test_report.program.runtime, false),
            " ]"
        );

        if (judge_test_report.checker) {
            tmplog(
                "  Checker: [ CPU: ",
                ::to_string(judge_test_report.checker->cpu_time, false),
                " RT: ",
                ::to_string(judge_test_report.checker->runtime, false),
                " ] ",
                judge_test_report.checker->peak_memory_in_bytes >> 10
            );
            tmplog(" / ", checker_mem_limit >> 10, " KiB");
        }
    }

    void group_score(int64_t score, int64_t max_score, double score_ratio) override {
        log("Score: ", score, " / ", max_score, " (ratio: ", ::to_string(score_ratio, 4), ')');
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

struct JudgeWorkerOptions {
    uint64_t max_executable_size_in_bytes = 32 << 20;
    std::chrono::nanoseconds checker_time_limit = std::chrono::seconds{10};
    uint64_t checker_memory_limit_in_bytes = 256 << 20;
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
};

/**
 * @brief Manages a judge worker
 * @details Only to use in ONE thread.
 */
class JudgeWorker {
    TemporaryDirectory tmp_dir{"/tmp/judge-worker.XXXXXX"};
    Simfile sf;
    std::unique_ptr<judge::language_suite::Suite> checker_suite;
    std::unique_ptr<judge::language_suite::Suite> solution_suite;

    std::unique_ptr<PackageLoader> package_loader;

    uint64_t max_executable_size_in_bytes;
    std::chrono::nanoseconds checker_time_limit;
    uint64_t checker_memory_limit_in_bytes;
    double score_cut_lambda; // has to be from [0, 1]

public:
    explicit JudgeWorker(JudgeWorkerOptions options = {});

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

    /// Compiles checker
    int compile_checker(
        std::chrono::nanoseconds time_limit,
        uint64_t compiler_memory_limit_in_bytes,
        std::string* c_errors,
        size_t c_errors_max_len,
        judge::CompilationCache* cache = nullptr,
        std::optional<std::string> cached_name = std::nullopt
    );

    /// Compiles solution
    int compile_solution(
        FilePath source,
        SolutionLanguage lang,
        std::chrono::nanoseconds time_limit,
        uint64_t compiler_memory_limit_in_bytes,
        std::string* c_errors,
        size_t c_errors_max_len,
        judge::CompilationCache* cache = nullptr,
        std::optional<std::string> cached_name = std::nullopt
    );

    /// Compiles solution
    /// @p source should be a path in package main dir e.g. If main dir ==
    /// "foo/" and solution has path "foo/bar/test", then @p source should be
    /// "bar/test"
    int compile_solution_from_package(
        FilePath source,
        SolutionLanguage lang,
        std::chrono::nanoseconds time_limit,
        uint64_t compiler_memory_limit_in_bytes,
        std::string* c_errors,
        size_t c_errors_max_len,
        judge::CompilationCache* cache = nullptr,
        std::optional<std::string> cached_name = std::nullopt
    ) {
        STACK_UNWINDING_MARK;
        auto solution_path = package_loader->load_as_file(source, "source");
        return compile_solution(
            solution_path,
            lang,
            time_limit,
            compiler_memory_limit_in_bytes,
            c_errors,
            c_errors_max_len,
            cache,
            std::move(cached_name)
        );
    }

    /**
     * @brief Runs solution with @p input_file as stdin and @p output_file as
     *   stdout (this function makes no sense for the interactive problems)
     *
     * @param input_file path to file to set as stdin
     * @param output_file path to file to set as stdout
     * @param time_limit time limit
     * @param memory_limit_in_bytes memory limit in bytes
     * @return exit status of running the solution (in the sandbox)
     */
    [[nodiscard]] sandbox::result::Ok run_solution(
        FilePath input_file,
        FilePath output_file,
        std::chrono::nanoseconds time_limit,
        uint64_t memory_limit_in_bytes
    ) const;

private:
    template <class Func>
    JudgeReport process_tests(
        bool final,
        JudgeLogger& judge_log,
        const std::optional<std::function<void(const JudgeReport&)>>& partial_report_callback,
        Func&& judge_on_test
    ) const;

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
        bool final,
        JudgeLogger& judge_log,
        const std::optional<std::function<void(const JudgeReport&)>>& partial_report_callback =
            std::nullopt
    ) const;

    [[nodiscard]] JudgeReport judge(bool final) const {
        VerboseJudgeLogger logger;
        return judge(final, logger);
    }
};

} // namespace sim
