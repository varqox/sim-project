#pragma once

#include <simlib/sim/judge_worker.hh>
#include <simlib/sim/simfile.hh>
#include <simlib/utilities.hh>
#include <type_traits>

namespace sim {

// Helps with converting a package to a Sim package
class Conver {
public:
    struct ReportBuff {
        std::string str;
        bool log_to_stdlog_;

        explicit ReportBuff(bool log_to_stdlog = false) : log_to_stdlog_(log_to_stdlog) {}

        template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
        void append(Args&&... args) {
            if (log_to_stdlog_) {
                stdlog(args...);
            }

            back_insert(str, std::forward<Args>(args)..., '\n');
        }
    };

private:
    ReportBuff report_;
    std::string package_path_;

public:
    [[nodiscard]] const std::string& package_path() const noexcept { return package_path_; }

    // @p path may point to a directory as well as a zip-package
    void package_path(std::string path) { package_path_ = std::move(path); }

    [[nodiscard]] const std::string& report() const noexcept { return report_.str; }

    struct ResetTimeLimitsOptions {
        // Minimum allowed time limit
        std::chrono::nanoseconds min_time_limit = std::chrono::milliseconds(300);

        // Solution runtime coefficient - the time limit will be set to
        // solution runtime * solution_runtime_coefficient + adjustment based
        // on minimum_time_limit.
        double solution_runtime_coefficient = 3;

        ResetTimeLimitsOptions() = default;
    };

    struct Options {
        // Leave unset to detect it from the Simfile in the package
        std::optional<std::string> name;
        // Leave unset to detect it from the Simfile in the package
        std::optional<std::string> label;
        // Whether the problem is interactive or not, leave unset to detect it
        // from Simfile in the package
        std::optional<bool> interactive;
        // In MiB. If set, overrides memory limit of every test
        std::optional<uint64_t> memory_limit;
        // If set, overrides time limit of every test (has lower precedence
        // than reset_time_limits_using_main_solution)
        std::optional<std::chrono::nanoseconds> global_time_limit;
        // Maximum allowed time limit on the test. If global_time_limit is set
        // this option is ignored
        std::chrono::nanoseconds max_time_limit = std::chrono::seconds(60);
        // If global_time_limit is set this option is ignored
        bool reset_time_limits_using_main_solution = false;
        // If true, Simfile in the package will be ignored
        bool ignore_simfile = false;
        // Whether to seek for new tests in the package
        bool seek_for_new_tests = false;
        // Whether to recalculate scoring
        bool reset_scoring = false;
        // Whether to throw an exception or give the warning if the statement
        // is not present
        bool require_statement = true;
        // Ignored if global_time_limit is set
        ResetTimeLimitsOptions rtl_opts;

        Options() = default;
    };

    enum class Status { COMPLETE, NEED_MODEL_SOLUTION_JUDGE_REPORT };

    struct ConstructionResult {
        Status status{};
        Simfile simfile;
        std::string pkg_main_dir;
    };

    /**
     * @brief Constructs Simfile based on extracted package and options @p opts
     * @details Constructs a valid Simfile. See description to each field in
     *   Options to learn more.
     *
     * @param opts options that parameterize Conver behavior
     * @param be_verbose whether to log report to the stdlog
     *
     * @return Status and a valid Simfile; If status == COMPLETE then Simfile
     *   is fully constructed - conver has finished its job. Otherwise it is
     *   necessary to run reset_time_limits_using_jugde_reports() method with
     *   the returned Simfile and a judge report of the main solution on the
     *   returned Simfile.
     *
     * @errors If any error is encountered then an exception of type
     *   std::runtime_error is thrown with a proper message
     */
    ConstructionResult construct_simfile(const Options& opts, bool be_verbose = false);

    /**
     * @brief Finishes constructing the Simfile partially constructed by the
     *   construct_simfile() method
     * @details Sets the time limits based on the main solution's judge report.
     *   Leaves the current time limit for tests that do not occur neither in
     *   @p jrep1 nor @p jrep2.
     *
     * @param sf Simfile to update (returned by construct_simfile())
     * @param jrep1 Initial judge report of the main solution, based on the
     *   @p sf.
     * @param jrep2 Final judge report of the main solution, based on the
     *   @p sf.
     * @param opts options that parameterize calculating time limits
     */
    static void reset_time_limits_using_jugde_reports(
        Simfile& sf,
        const JudgeReport& jrep1,
        const JudgeReport& jrep2,
        const ResetTimeLimitsOptions& opts
    );

    static constexpr std::chrono::duration<double> solution_runtime_to_time_limit(
        std::chrono::duration<double> main_solution_runtime,
        double solution_runtime_coefficient,
        std::chrono::duration<double> min_time_limit
    ) noexcept {
        double x = main_solution_runtime.count();
        return solution_runtime_coefficient * main_solution_runtime +
            min_time_limit * 2 / (x * x + 2);
    }

    static constexpr std::chrono::duration<double> time_limit_to_solution_runtime(
        std::chrono::duration<double> time_limit,
        double solution_runtime_coefficient,
        std::chrono::duration<double> min_time_limit
    ) noexcept {
        using DS = std::chrono::duration<double>;
        // Use Newton method, as it is simpler than solving the equation
        double x = time_limit.count();
        for (;;) {
            double fx =
                (time_limit -
                 solution_runtime_to_time_limit(DS(x), solution_runtime_coefficient, min_time_limit)
                )
                    .count();
            if (-1e-9 < fx and fx < 1e-9) {
                return DS(x);
            }

            double dfx = -solution_runtime_coefficient +
                4 * min_time_limit.count() * x / (x * x + 2) / (x * x + 2);
            x = x - fx / dfx;
        }
    }

private:
    // Rounds time limits to 0.01 s
    static void normalize_time_limits(Simfile& sf) {
        using namespace std::chrono_literals;
        for (auto&& g : sf.tgroups) {
            for (auto&& t : g.tests) {
                t.time_limit = floor_to_10ms(t.time_limit + 5ms);
            }
        }
    };
};

} // namespace sim
