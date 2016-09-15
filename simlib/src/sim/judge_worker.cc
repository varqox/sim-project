#include "../../include/parsers.h"
#include "../../include/sim/checker.h"
#include "../../include/sim/judge_worker.h"

using std::string;

namespace sim {

constexpr meta::string JudgeWorker::CHECKER_FILENAME;
constexpr meta::string JudgeWorker::SOLUTION_FILENAME;

JudgeReport JudgeWorker::judge(bool final) const {
	#if __cplusplus > 201103L
	# warning "Use variadic generic lambda instead"
	#endif

	#define LOG(...) do { if (verbose) stdlog(__VA_ARGS__); } while (false)

	LOG("Judging on `", pkg_root,"` (", (final ? "final" : "initial"), "): {");

	string sol_stdout_path {tmp_dir.path() + "sol_stdout"};
	// Checker STDOUT
	FileDescriptor checker_stdout {openUnlinkedTmpFile()};
	if (checker_stdout < 0)
		THROW("Failed to create unlinked temporary file", error(errno));

	// Solution STDOUT
	FileDescriptor solution_stdout(sol_stdout_path,
		O_WRONLY | O_CREAT | O_TRUNC);
	if (solution_stdout < 0)
		THROW("Failed to open file `", sol_stdout_path, '`', error(errno));

	FileRemover solution_stdout_remover {sol_stdout_path}; // Save disk space

	const int page_size = sysconf(_SC_PAGESIZE);

	// Checker parameters
	Sandbox::Options checker_opts = {
		-1, // STDIN is ignored
		checker_stdout, // STDOUT
		-1, // STDERR is ignored
		CHECKER_TIME_LIMIT,
		CHECKER_MEMORY_LIMIT + page_size // To be able to detect exceeding
	};

	JudgeReport report;
	string checker_path {concat(tmp_dir.path(), CHECKER_FILENAME)};
	string solution_path {concat(tmp_dir.path(), SOLUTION_FILENAME)};

	for (auto&& group : sf.tgroups) {
		// Group "0" goes to the initial report, others groups to final
		auto p = Simfile::TestNameComparator::split(group.tests[0].name);
		if ((p.first != "0") != final)
			continue;

		report.groups.emplace_back();
		auto& report_group = report.groups.back();

		double score_ratio = 1.0;

		for (auto&& test : group.tests) {
			// Prepare solution fds
			(void)ftruncate(solution_stdout, 0);
			(void)lseek(solution_stdout, 0, SEEK_SET);

			string test_in_path {pkg_root + test.in};
			string test_out_path {pkg_root + test.out};

			FileDescriptor test_in(test_in_path, O_RDONLY | O_LARGEFILE);
			if (test_in < 0)
				THROW("Failed to open file `", test_in_path, '`', error(errno));

			// Run solution on the test
			Sandbox::ExitStat es = Sandbox::run(solution_path, {}, {
				test_in,
				solution_stdout,
				-1,
				test.time_limit,
				test.memory_limit + page_size, // To be able to detect exceeding
			}); // Allow exceptions to fly upper

			// Log syscalls problems
			if (verbose && hasPrefixIn(es.message,
				{"Error: ", "failed to get syscall", "forbidden syscall"}))
			{
				errlog("Package: `", pkg_root, '`', test.name, " -> ",
					es.message);
			}

			// Update score_ratio
			score_ratio = std::min(score_ratio,
				2.0 - 2.0 * (es.runtime / 10000) / (test.time_limit / 10000));

			report_group.tests.emplace_back(
				test.name,
				JudgeReport::Test::OK,
				es.runtime,
				test.time_limit,
				es.vm_peak,
				test.memory_limit,
				std::string {}
			);
			auto& test_report = report_group.tests.back();

			auto do_logging = [&] {
				if (!verbose)
					return;

				auto tmplog = stdlog("  ", widedString(test.name, 11, LEFT),
					widedString(usecToSecStr(test_report.runtime, 2, false), 4),
					" / ", usecToSecStr(test_report.time_limit, 2, false),
					" s  ", toStr(test_report.memory_consumed >> 10), " / ",
					toStr(test_report.memory_limit >> 10), " KB"
					"    Status: \033[1;33m");
				// Status
				switch (test_report.status) {
				case JudgeReport::Test::TLE: tmplog("TLE\033[m"); break;
				case JudgeReport::Test::MLE: tmplog("MLE\033[m"); break;
				case JudgeReport::Test::RTE: tmplog("RTE\033[m (", es.message,
					')'); break;
				default:
					THROW("Should not reach here");
				}
				// Rest
				tmplog("   Exited with ", toStr(es.code), " [ ",
					usecToSecStr(es.runtime, 6, false), " ]");
			};

			// OK
			if (es.code == 0) {

			// TLE
			} else if (test_report.runtime >= test_report.time_limit) {
				// After checking status for OK `>=` comparison is safe to
				// detect exceeding
				score_ratio = 0;
				test_report.status = JudgeReport::Test::TLE;
				test_report.comment = "Time limit exceeded";
				do_logging();
				continue;

			// MLE
			} else if (es.message == "Memory limit exceeded" ||
				test_report.memory_consumed > test_report.memory_limit)
			{
				score_ratio = 0;
				test_report.status = JudgeReport::Test::MLE;
				test_report.comment = "Memory limit exceeded";
				do_logging();
				continue;

			// RTE
			} else {
				score_ratio = 0;
				test_report.status = JudgeReport::Test::RTE;
				test_report.comment = "Runtime error";
				if (es.message.size())
					back_insert(test_report.comment, " (", es.message, ')');

				do_logging();
				continue;
			}

			// Status == OK

			/* Checking solution output with checker */

			// Prepare checker fds
			(void)ftruncate(checker_stdout, 0);
			(void)lseek(checker_stdout, 0, SEEK_SET);

			// Run checker
			es = Sandbox::run(checker_path,
				{checker_path, test_in_path, test_out_path, sol_stdout_path},
				checker_opts,
				".",
				CheckerCallback({test_in_path, test_out_path, sol_stdout_path})
			); // Allow exceptions to fly upper

			// Checker exited with 0
			if (es.code == 0) {
				string chout = sim::obtainCheckerOutput(checker_stdout, 512);
				SimpleParser s {chout};

				StringView line1 {s.extractNext('\n')}; // "OK" or "WRONG"
				StringView line2 {s.extractNext('\n')}; // percentage (real)
				// Second line has to be either empty or be a real number
				if (line2.size() && (line2[0] == '-' || !isReal(line2))) {
					score_ratio = 0; // Do not give score for a checker error
					test_report.status = JudgeReport::Test::CHECKER_ERROR;
					test_report.comment = concat("Checker error: second line "
						"of the checker stdout is invalid: `", line2, '`');

				// OK -> Checker: OK
				} else if (line1 == "OK") {
					// Test status is already set to OK
					if (line2.size()) // Empty line means 100%
						score_ratio = std::min(score_ratio,
							atof(line2.to_string().data()) * 0.01); /*
								.to_string() is safer than tricky indexing over
								chout */

					// Leave the checker comment only
					chout.erase(chout.begin(), chout.end() - s.size());
					test_report.comment = std::move(chout);

				// WRONG -> Checker WA
				} else {
					test_report.status = JudgeReport::Test::WA;
					score_ratio = 0;
					// Leave the checker comment only
					chout.erase(chout.begin(), chout.end() - s.size());
					test_report.comment = std::move(chout);
				}

			// Checker TLE
			} else if (es.runtime >= CHECKER_TIME_LIMIT) {
				score_ratio = 0; // Do not give score for a checker error
				test_report.status = JudgeReport::Test::CHECKER_ERROR;
				test_report.comment = "Checker: time limit exceeded";

			// Checker MLE
			} else if (es.message == "Memory limit exceeded" ||
				es.vm_peak > CHECKER_MEMORY_LIMIT)
			{
				score_ratio = 0; // Do not give score for a checker error
				test_report.status = JudgeReport::Test::CHECKER_ERROR;
				test_report.comment = "Checker: memory limit exceeded";

			// Checker RTE
			} else {
				score_ratio = 0; // Do not give score for a checker error
				test_report.status = JudgeReport::Test::CHECKER_ERROR;
				test_report.comment = "Checker runtime error";
				if (es.message.size())
					back_insert(test_report.comment, " (", es.message, ')');
			}

			if (verbose) {
				auto tmplog = stdlog("  ", widedString(test.name, 11, LEFT),
					widedString(usecToSecStr(test_report.runtime, 2, false), 4),
					" / ", usecToSecStr(test_report.time_limit, 2, false),
					" s  ", toStr(test_report.memory_consumed >> 10), " / ",
					toStr(test_report.memory_limit >> 10), " KB"
					"    Status: \033[1;32mOK\033[m   Exited with 0 [ ",
					usecToSecStr(test_report.runtime, 6, false), " ]  "
					"Checker: ");
				// Checker status
				if (test_report.status == JudgeReport::Test::OK)
					tmplog("\033[1;32mOK\033[m  ", test_report.comment);
				else if (test_report.status == JudgeReport::Test::WA)
					tmplog("\033[1;31mWRONG\033[m ", test_report.comment);
				else
					tmplog("\033[1;33mERROR\033[m ", test_report.comment);
				// Rest
				tmplog("   Exited with ", toStr(es.code), " [ ",
					usecToSecStr(es.runtime, 6, false), " ]  ",
					toStr(es.vm_peak >> 10), " / ",
					toStr(CHECKER_MEMORY_LIMIT >> 10), " KB");
			}
		}

		// Compute group score
		report_group.score = round(group.score * score_ratio);
		report_group.max_score = group.score;

		LOG("  Score: ", toStr(report_group.score), " / ",
			toStr(report_group.max_score), " (ratio: ", toStr(score_ratio, 3),
			')');
	}

	LOG('}');
	return report;
}

} // namespace sim
