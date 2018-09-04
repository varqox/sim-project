#pragma once

#include "../filesystem.h"
#include "../utilities.h"
#include "judge_worker.h"
#include "simfile.h"

namespace sim {

// Helps with converting a package to a Sim package
class Conver {
public:
	struct ReportBuff {
		std::string str;
		bool log_to_stdlog_;

		ReportBuff(bool log_to_stdlog = false)
			: log_to_stdlog_(log_to_stdlog) {}

		template<class... Args>
		void append(Args&&... args) {
			if (log_to_stdlog_)
				stdlog(args...);

			back_insert(str, std::forward<Args>(args)..., '\n');
		}
	};

private:
	ReportBuff report_;
	std::string package_path_;

public:
	const std::string& getPackagePath() const noexcept { return package_path_; }

	// @p path may point to a directory as well as a zip-package
	void setPackagePath(std::string path) {	package_path_ = std::move(path); }

	const std::string& getReport() const noexcept { return report_.str; }

	struct Options {
		// Leave empty to detect it from the Simfile in the package
		std::string name;
		// Leave empty to detect it from the Simfile in the package
		std::string label;
		// In MB. Set to a non-zero value to override memory limit of every test
		uint64_t memory_limit = 0;
		// In microseconds. Set to a non-zero value to override time limit of
		// every test (has lower precedence than
		// reset_time_limits_using_model_solution)
		uint64_t global_time_limit = 0;
		// Maximum allowed time limit on the test in microseconds. If
		// global_time_limit != 0 this option is ignored
		uint64_t max_time_limit = 60e6;
		// If global_time_limit != 0 this option is ignored
		bool reset_time_limits_using_model_solution = false;
		// If true, Simfile in the package will be ignored
		bool ignore_simfile = false;
		// Whether to seek for new tests in the package
		bool seek_for_new_tests = false;
		// Whether to recalculate scoring
		bool reset_scoring = false;
		// Whether to throw an exception or give the warning if the statement is
		// not present
		bool require_statement = true;

		Options() = default;
	};

	enum class Status { COMPLETE, NEED_MODEL_SOLUTION_JUDGE_REPORT };

	struct ConstructionResult {
		Status status;
		Simfile simfile;
		std::string pkg_master_dir;
	};

	/**
	 * @brief Constructs Simfile based on extracted package and options @p opts
	 * @details Constructs a valid Simfile. See description to each field in
	 *   Options to learn more.
	 *
	 * @param opts options that parameterize Conver behavior
	 *
	 * @return Status and a valid Simfile; If status == COMPLETE then Simfile is
	 *   fully constructed - conver has finished its job. Otherwise it is
	 *   necessary to run finishConstructingSimfile() method with the returned
	 *   Simfile and a judge report of the model solution on the returned
	 *   Simfile.
	 *
	 * @errors If any error is encountered then an exception of type
	 *   std::runtime_error is thrown with a proper message
	 */
	ConstructionResult constructSimfile(const Options& opts);

	 /**
	  * @brief Finishes constructing the Simfile partially constructed by the
	  *   constructSimfile() method
	  * @details Sets the time limits based on the model solution's judge report
	  *
	  * @param sf Simfile to update (returned by constructSimfile())
	  * @param jrep1 Initial judge report of the model solution, based on the
	  *   @p sf. Passing @p jrep1 that is not based on the @p sf is
	  *   undefined-behavior.
	  * @param jrep2 Final judge report of the model solution, based on the
	  *   @p sf. Passing @p jrep2 that is not based on the @p sf is
	  *   undefined-behavior.
	  */
	void finishConstructingSimfile(Simfile& sf, const JudgeReport& jrep1,
		const JudgeReport& jrep2);

private:
	static constexpr double time_limit(double model_solution_runtime) {
		double x = model_solution_runtime;
		return 3*x + 2/(2*x*x + 5);
	}

	static constexpr uint64_t MODEL_SOLUTION_TIME_LIMIT = 20e6;

	// Rounds time limits to 0.01 s
	static void normalize_time_limits(Simfile& sf) {
		constexpr unsigned GRANULARITY = 0.01e6; // 0.01s
		for (auto&& g : sf.tgroups)
			for (auto&& t : g.tests)
				t.time_limit = (t.time_limit + GRANULARITY / 2) / GRANULARITY *
					GRANULARITY;
	};
};

} // namespace sim
