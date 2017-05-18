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

		template<class... Args>
		void append(Args&&... args) {
			back_insert(str, std::forward<Args>(args)..., '\n');
		}
	};

private:
	ReportBuff report_;
	std::string package_path_;
	bool verbose_ = false;

public:
	bool getVerbosity() const noexcept { return verbose_; }

	void setVerbosity(bool verbosity) noexcept { verbose_ = verbosity; }

	const std::string& getPackagePath() const noexcept { return package_path_; }

	void setPackagePath(const std::string& path) {
		package_path_ = path;
		if (path.size() && path.back() != '/')
			package_path_ += '/';
	}

	const std::string& getReport() const noexcept { return report_.str; }

	struct Options {
		std::string name; // Leave empty to detect it from the Simfile in the
		                  // package
		std::string label; // Leave empty to detect it from the Simfile in the
		                   // package
		uint64_t memory_limit = 0; // [MB] Set to a non-zero value to overwrite
		                           // memory limit of every test with this value
		uint64_t global_time_limit = 0; // [microseconds] If non-zero then every
		                                // test will get this as time limit
		uint64_t max_time_limit = 60e6; // [microseconds] Maximum allowed time
		                                // limit on the test. If
		                                // global_time_limit != 0 this option is
		                                // ignored
		bool ignore_simfile = false; // If true, Simfile in the package will be
		                             // ignored
		bool force_auto_time_limits_setting =
			false; // If global_time_limit != 0 this option is ignored
		uint64_t compilation_time_limit = 30e6; // [microseconds] for checker
		                                        // and solution compilation
		uint64_t compilation_errors_max_length = 32768; // [bytes]
		std::string proot_path = "proot"; // Path to PRoot (passed to
		                                  // Simfile::compile* methods)

		Options() = default;
	};

	enum class Status { COMPLETE, NEED_MODEL_SOLUTION_JUDGE_REPORT };
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
	std::pair<Status, Simfile> constructSimfile(const Options& opts);

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
	static constexpr double time_limit(double model_sol_runtime) {
		double x = model_sol_runtime;
		return 3*x + 2/(2*x*x + 5);
	}

	static constexpr uint64_t MODEL_SOL_TLIMIT = 20e6;

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
