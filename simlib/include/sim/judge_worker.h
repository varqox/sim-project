#pragma once

#include "../debug.h"
#include "../filesystem.h"
#include "compile.h"
#include "simfile.h"

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
		};

		static const char* description(Status st) {
			switch (st) {
			case OK: return "OK";
			case WA: return "Wrong answer";
			case TLE: return "Time limit exceeded";
			case MLE: return "Memory limit exceeded";
			case RTE: return "Runtime error";
			case CHECKER_ERROR: return "Checker error";
			}

			return "Unknown"; // Should not happen but GCC complains about it
		}

		std::string name;
		Status status;
		uint64_t runtime, time_limit;
		uint64_t memory_consumed, memory_limit;
		std::string comment;

		Test(std::string n, Status s, uint64_t rt, uint64_t tl, uint64_t mc,
				uint64_t ml, std::string c)
			: name(std::move(n)), status(s), runtime(rt), time_limit(tl),
			memory_consumed(mc), memory_limit(ml), comment(std::move(c))
		{}
	};

	struct Group {
		std::vector<Test> tests;
		int64_t score, max_score; // score belongs to [0, max_score], or if
		                          // max_score is negative, to [max_score, 0]
	};

	std::vector<Group> groups;
};

/**
 * @brief Manages a judge worker
 * @details Only to use in ONE thread.
 */
class JudgeWorker {
	TemporaryDirectory tmp_dir {"/tmp/judge-worker.XXXXXX"};
	Simfile sf;
	std::string pkg_root; // with terminating '/'
	bool verbose = false;

	constexpr static meta::string CHECKER_FILENAME {"checker"};
	constexpr static meta::string SOLUTION_FILENAME {"solution"};
	constexpr static uint64_t CHECKER_TIME_LIMIT = 10e6; // 10 s
	constexpr static uint64_t CHECKER_MEMORY_LIMIT = 256 << 20; // 256 MiB

public:
	JudgeWorker() = default;

	JudgeWorker(const JudgeWorker&) = delete;
	JudgeWorker(JudgeWorker&&) noexcept = default;
	JudgeWorker& operator=(const JudgeWorker&) = delete;
	JudgeWorker& operator=(JudgeWorker&&) = default;

	/// Returns whether verbose
	bool getVerbosity() const { return verbose; }

	/// Set whether to be verbose
	void setVerbosity(bool verbosity) { verbose = verbosity; }

	/// Loads package from @p simfile in @p package_path
	void loadPackage(std::string package_path, std::string simfile) {
		sf = Simfile {std::move(simfile)};
		sf.loadChecker();
		sf.loadTestsWithFiles();

		pkg_root = std::move(package_path);
		if (!isDirectory(pkg_root))
			THROW("Invalid path to package root directory");

		if (pkg_root.back() != '/')
			pkg_root += '/';
	}

	/// Compiles checker, as sim::compile()
	int compileChecker(uint64_t time_limit, std::string* c_errors = nullptr,
		size_t c_errors_max_len = -1)
	{
		return compile(pkg_root + sf.checker,
			concat(tmp_dir.path(), CHECKER_FILENAME), verbose, time_limit,
			c_errors, c_errors_max_len);
	}

	/// Compiles checker, as sim::compile()
	int compileChecker(uint64_t time_limit, std::string* c_errors,
		size_t c_errors_max_len, const std::string& proot_path)
	{
		return compile(pkg_root + sf.checker,
			concat(tmp_dir.path(), CHECKER_FILENAME), verbose, time_limit,
			c_errors, c_errors_max_len, proot_path);
	}

	/// Compiles solution, as sim::compile()
	int compileSolution(const std::string& source, uint64_t time_limit,
		std::string* c_errors = nullptr, size_t c_errors_max_len = -1)
	{
		return compile(source, concat(tmp_dir.path(), SOLUTION_FILENAME),
			verbose, time_limit, c_errors, c_errors_max_len);
	}

	/// Compiles solution, as sim::compile()
	int compileSolution(const std::string& source, uint64_t time_limit,
		std::string* c_errors, size_t c_errors_max_len,
		const std::string& proot_path)
	{
		return compile(source, concat(tmp_dir.path(), SOLUTION_FILENAME),
			verbose, time_limit, c_errors, c_errors_max_len, proot_path);
	}

	/**
	 * @brief Judges last compiled solution on the last loaded package.
	 * @details Before calling this method, the methods loadPackage(),
	 *   compileChecker() and compileSolution() have to be called.
	 *
	 * @param final Whether judge only on final tests or only initial tests
	 * @return Judge report
	 */
	JudgeReport judge(bool final) const;
};

} // namespace sim
