#pragma once

#include "../debug.h"
#include "../filesystem.h"
#include "../sandbox.h"
#include "../utilities.h"
#include "../zip.h"
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

		constexpr static CStringView description(Status st) {
			switch (st) {
			case OK: return CStringView{"OK"};
			case WA: return CStringView{"Wrong answer"};
			case TLE: return CStringView{"Time limit exceeded"};
			case MLE: return CStringView{"Memory limit exceeded"};
			case RTE: return CStringView{"Runtime error"};
			case CHECKER_ERROR: return CStringView{"Checker error"};
			}

			// Should not happen but GCC complains about it
			return CStringView{"Unknown"};
		}

		std::string name;
		Status status;
		uint64_t runtime, time_limit;
		uint64_t memory_consumed, memory_limit; // in bytes
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
	std::string judge_log;

	/**
	 * @brief Returns the pretty-printed judge report
	 *
	 * @param span_status Should take Test::Status as an argument and return
	 *   something that will be appended to the dump as the test's status.
	 *
	 * @return A pretty-printed judge report
	 */
	template<class Func>
	std::string pretty_dump(Func&& span_status) const {
		std::string res = "{\n";
		for (auto&& group : groups) {
			for (auto&& test : group.tests) {
				back_insert(res, "  ", paddedString(test.name, 11, LEFT),
					paddedString(usecToSecStr(test.runtime, 2, false), 4),
					" / ", usecToSecStr(test.time_limit, 2, false),
					" s  ", test.memory_consumed >> 10, " / ",
					test.memory_limit >> 10, " KB"
					"    Status: ");
				// Status
				res += span_status(test.status);

				// Comment
				if (test.comment.size())
					back_insert(res, ' ', test.comment, '\n');
				else
					res += '\n';
			}

			// Score
			back_insert(res, "  Score: ", group.score, " / ", group.max_score,
				'\n');
		}

		return res += '}';
	}

	/// Returns the pretty-printed judge report
	std::string pretty_dump() const {
		return pretty_dump([](Test::Status status) {
			switch (status) {
			case Test::OK: return "OK";
			case Test::WA: return "WA";
			case Test::TLE: return "TLE";
			case Test::MLE: return "MLE";
			case Test::RTE: return "RTE";
			case Test::CHECKER_ERROR: return "CHECKER_ERROR";
			}

			return "UNKNOWN";
		});
	}
};

enum class SolutionLanguage {
	UNKNOWN, C, CPP, PASCAL
};

inline bool is_source(StringView file) noexcept {
	return hasSuffixIn(file, {".c", ".cc", ".cpp", ".cxx", ".pas"});
}

inline SolutionLanguage filename_to_lang(StringView filename) {
	SolutionLanguage res = SolutionLanguage::UNKNOWN;
	if (hasSuffix(filename, ".c"))
		res = SolutionLanguage::C;
	else if (hasSuffixIn(filename, {".cc", ".cpp", ".cxx"}))
		res = SolutionLanguage::CPP;
	else if (hasSuffix(filename, ".pas"))
		res = SolutionLanguage::PASCAL;

	// It is written that way, so the compiler will warn when the enum changes
	switch (res) {
	case SolutionLanguage::UNKNOWN:
	case SolutionLanguage::C:
	case SolutionLanguage::CPP:
	case SolutionLanguage::PASCAL:
	// If missing one, then update above ifs
		return res;
	}

	THROW("Should not reach here");
}

class JudgeLogger {
protected:
	std::string log_;

public:
	virtual void begin(StringView package_path, bool final) = 0;

	virtual void test(StringView test_name, JudgeReport::Test test_report,
		Sandbox::ExitStat es) = 0;

	virtual void test(StringView test_name, JudgeReport::Test test_report,
		Sandbox::ExitStat es, Sandbox::ExitStat checker_es,
		uint64_t checker_mem_limit, StringView checker_error_str) = 0;

	virtual void group_score(int64_t score, int64_t max_score, double score_ratio) = 0;

	virtual void end() = 0;

	std::string&& render_judge_log() { return std::move(log_); }

	virtual ~JudgeLogger() = default;
};

class VerboseJudgeLogger : public JudgeLogger {
	Logger dummy_logger_;
	Logger& logger_;

	int64_t total_score_;
	int64_t max_total_score_;

	template<class... Args>
	auto log(Args&&... args) {
		return DoubleAppender<decltype(log_)>(logger_, log_,
			std::forward<Args>(args)...);
	}

	template<class Func>
	void log_test(StringView test_name, JudgeReport::Test test_report,
		Sandbox::ExitStat es, Func&& func)
	{
		auto tmplog = log("  ", paddedString(test_name, 11, LEFT),
			paddedString(usecToSecStr(test_report.runtime, 2, false), 4),
			" / ", usecToSecStr(test_report.time_limit, 2, false),
			" s  ", test_report.memory_consumed >> 10, " / ",
			test_report.memory_limit >> 10, " KB  Status: ");
		// Status
		switch (test_report.status) {
		case JudgeReport::Test::TLE: tmplog("\033[1;33mTLE\033[m");
			break;
		case JudgeReport::Test::MLE: tmplog("\033[1;33mMLE\033[m");
			break;
		case JudgeReport::Test::RTE:
			tmplog("\033[1;31mRTE\033[m (", es.message, ')');
			break;
		case JudgeReport::Test::OK:
			tmplog("\033[1;32mOK\033[m");
			break;
		case JudgeReport::Test::WA:
			tmplog("\033[1;31mWA\033[m");
			break;
		case JudgeReport::Test::CHECKER_ERROR:
			tmplog("\033[1;35mCHECKER ERROR\033[m (Running \033[1;32mOK\033[m)");
			break;
		}

		// Rest
		tmplog(" [ CPU: ", timespec_to_str(es.cpu_runtime, 9, false),
			" RT: ", timespec_to_str(es.runtime, 9, false), " ]");

		func(tmplog);
	}

public:
	VerboseJudgeLogger(bool log_to_stdlog = false)
		: dummy_logger_(nullptr), logger_(log_to_stdlog ? stdlog : dummy_logger_) {}

	void begin(StringView package_path, bool final) override {
		total_score_ = max_total_score_ = 0;
		log("Judging on `", package_path,"` (", (final ? "final" : "initial"),
			"): {");
	}

	void test(StringView test_name, JudgeReport::Test test_report,
		Sandbox::ExitStat es) override
	{
		log_test(test_name, test_report, es, [](auto&){});
	}

	void test(StringView test_name, JudgeReport::Test test_report,
		Sandbox::ExitStat es, Sandbox::ExitStat checker_es, uint64_t checker_mem_limit, StringView checker_error_str) override
	{
		log_test(test_name, test_report, es, [&](auto& tmplog) {
			tmplog("  Checker: ");

			// Checker status
			if (test_report.status == JudgeReport::Test::OK)
				tmplog("\033[1;32mOK\033[m ", test_report.comment);
			else if (test_report.status == JudgeReport::Test::WA)
				tmplog("\033[1;31mWA\033[m ", test_report.comment);
			else if (test_report.status == JudgeReport::Test::CHECKER_ERROR)
				tmplog("\033[1;35mERROR\033[m ", checker_error_str);
			else
				return; // Checker was not run


			tmplog(" [ RT: ", timespec_to_str(checker_es.runtime, 9, false), " ] ",
				checker_es.vm_peak >> 10, " / ", checker_mem_limit >> 10, " KB");
		});
	}

	void group_score(int64_t score, int64_t max_score, double score_ratio) override {
		total_score_ += score;
		if (max_score > 0)
			max_total_score_ += max_score;

		log("Score: ", score, " / ", max_score, " (ratio: ", toStr(score_ratio, 4), ')');
	}

	void end() override {
		if (total_score_ != 0 or max_total_score_ != 0)
			log("Total score: ", total_score_, " / ", max_total_score_);
		log('}');
	}
};

/**
 * @brief Manages a judge worker
 * @details Only to use in ONE thread.
 */
class JudgeWorker {
	TemporaryDirectory tmp_dir {"/tmp/judge-worker.XXXXXX"};
	Simfile sf;
	std::string pkg_root; // with terminating '/'

	constexpr static meta::string CHECKER_FILENAME {"checker"};
	constexpr static meta::string SOLUTION_FILENAME {"solution"};
	constexpr static timespec CHECKER_TIME_LIMIT = {10, 0}; // 10 s
	constexpr static uint64_t CHECKER_MEMORY_LIMIT = 256 << 20; // 256 MiB

public:
	JudgeWorker() = default;

	JudgeWorker(const JudgeWorker&) = delete;
	JudgeWorker(JudgeWorker&&) noexcept = default;
	JudgeWorker& operator=(const JudgeWorker&) = delete;
	JudgeWorker& operator=(JudgeWorker&&) = default;

	/// Loads package from @p simfile in @p package_path
	void loadPackage(std::string package_path, std::string simfile);

 	// Returns a const reference to the package's Simfile
	const Simfile& simfile() const noexcept { return sf; }

private:
	int compile_impl(CStringView source, SolutionLanguage lang,
		timespec time_limit, std::string* c_errors,
		size_t c_errors_max_len, const std::string& proot_path,
		StringView compilation_source_basename, StringView exec_dest_filename);

public:
	/// Compiles checker (using sim::compile())
	int compileChecker(timespec time_limit, std::string* c_errors,
		size_t c_errors_max_len, const std::string& proot_path)
	{
		return compile_impl(concat(pkg_root, sf.checker).to_cstr(),
			filename_to_lang(sf.checker), time_limit, c_errors,
			c_errors_max_len, proot_path, "checker", CHECKER_FILENAME);
	}

	/// Compiles solution (using sim::compile())
	int compileSolution(CStringView source, SolutionLanguage lang,
		timespec time_limit, std::string* c_errors,
		size_t c_errors_max_len, const std::string& proot_path)
	{
		return compile_impl(source, lang, time_limit, c_errors,
			c_errors_max_len, proot_path, "source", SOLUTION_FILENAME);
	}

	void loadCompiledChecker(CStringView compiled_checker) {
		if (copy(compiled_checker,
			concat<PATH_MAX>(tmp_dir.path(), CHECKER_FILENAME).to_cstr(), S_0755))
		{
			THROW("copy()", errmsg());
		}
	}

	void loadCompiledSolution(CStringView compiled_solution) {
		if (copy(compiled_solution,
			concat<PATH_MAX>(tmp_dir.path(), SOLUTION_FILENAME).to_cstr(), S_0755))
		{
			THROW("copy()", errmsg());
		}
	}

	void saveCompiledChecker(CStringView destination) {
		if (copy(concat<PATH_MAX>(tmp_dir.path(), CHECKER_FILENAME).to_cstr(),
			destination, S_0755))
		{
			THROW("copy()", errmsg());
		}
	}

	void saveCompiledSolution(CStringView destination) {
		if (copy(concat<PATH_MAX>(tmp_dir.path(), SOLUTION_FILENAME).to_cstr(),
			destination, S_0755))
		{
			THROW("copy()", errmsg());
		}
	}

	/**
	 * @brief Runs solution with @p input_file as stdin and @p output_file as
	 *   stdout
	 *
	 * @param input_file path to file to set as stdin
	 * @param output_file path to file to set as stdout
	 * @param time_limit time limit in usec
	 * @param memory_limit memory limit in bytes
	 * @return exit status of running the solution (in the sandbox)
	 */
	Sandbox::ExitStat run_solution(CStringView input_file,
		CStringView output_file, uint64_t time_limit, uint64_t memory_limit)
		const;

	/**
	 * @brief Judges last compiled solution on the last loaded package.
	 * @details Before calling this method, the methods loadPackage(),
	 *   compileChecker() and compileSolution() have to be called.
	 *
	 * @param final Whether judge only on final tests or only initial tests
	 * @return Judge report
	 */
	JudgeReport judge(bool final, JudgeLogger&& judge_log) const;

	JudgeReport judge(bool final) const {
		return judge(final, VerboseJudgeLogger());
	}
};

} // namespace sim
