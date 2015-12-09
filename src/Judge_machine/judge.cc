#include "judge.h"
#include "main.h"

#include "../simlib/include/compile.h"
#include "../simlib/include/logger.h"
#include "../simlib/include/sandbox.h"
#include "../simlib/include/sandbox_checker_callback.h"
#include "../simlib/include/sim_problem.h"
#include "../simlib/include/utility.h"

using std::string;
using std::vector;

constexpr size_t COMPILE_ERRORS_MAX_LENGTH = 100 << 10; // 100kB

namespace {

struct TestResult {
	const string* name;
	enum Status { ERROR, CHECKER_ERROR, OK, WA, TLE, RTE } status;
	string time;

	static string statusToTdString(Status s) {
		switch (s) {
		case ERROR:
			return "<td class=\"wa\">Error</td>";
		case CHECKER_ERROR:
			return "<td class=\"judge-error\">Checker error</td>";
		case OK:
			return "<td class=\"ok\">OK</td>";
		case WA:
			return "<td class=\"wa\">Wrong answer</td>";
		case TLE:
			return "<td class=\"tl-rte\">Time limit exceeded</td>";
		case RTE:
			return "<td class=\"tl-rte\">Runtime error</td>";
		}

		return "<td>Unknown</td>";
	}
};

} // anonymous namespace

JudgeResult judge(string submission_id, string problem_id) {
	string package_path = concat("problems/", problem_id, '/');

	// Load config
	ProblemConfig pconf;
	try {
		if (VERBOSITY > 1)
			stdlog("Validating config.conf...");
		pconf.loadConfig(package_path);
		if (VERBOSITY > 1)
			stdlog("Validation passed.");

	} catch (std::exception& e) {
		if (VERBOSITY > 0)
			error_log("Error: ", e.what());
		return JudgeResult(JudgeResult::JUDGE_ERROR, 0);
	}

	// Compile solution
	try {
		string compile_errors;
		if (0 != compile(concat("solutions/", submission_id, ".cpp"),
				concat(tmp_dir.sname(), "exec"), (VERBOSITY >> 1) + 1,
				&compile_errors,COMPILE_ERRORS_MAX_LENGTH, "./proot"))
			return JudgeResult(JudgeResult::COMPILE_ERROR, 0,
				htmlSpecialChars(compile_errors));

		// Compile checker
		if (0 != compile(concat(package_path, "check/", pconf.checker),
				concat(tmp_dir.sname(), "checker"), (VERBOSITY >> 1) + 1,
				nullptr, 0, "./proot"))
			return JudgeResult(JudgeResult::COMPILE_ERROR, 0,
				"Checker compilation error");

	} catch (const std::exception& e) {
		stdlog("Compilation error", e.what());
		error_log("Compilation error", e.what());
		return JudgeResult(JudgeResult::COMPILE_ERROR, 0,
			"Judge machine error");
	}

	// Prepare runtime environment
	sandbox::options sb_opts = {
		0, // Will be set separately for each test later
		pconf.memory_limit << 10,
		-1,
		open(concat(tmp_dir.sname(), "answer").c_str(),
			O_WRONLY | O_CREAT | O_TRUNC,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), // (mode: 0644/rw-r--r--)
		-1
	};

	// Check for errors
	if (sb_opts.new_stdout_fd < 0) {
		error_log("Failed to open '", tmp_dir.name(), "answer'",
			error(errno));
		return JudgeResult(JudgeResult::JUDGE_ERROR, 0);
	}

	sandbox::options checker_sb_opts = {
		10 * 1000000, // 10s
		256 << 20, // 256 MB
		-1,
		open(concat(tmp_dir.sname(), "checker_out").c_str(),
			O_WRONLY | O_CREAT | O_TRUNC,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), // (mode: 0644/rw-r--r--)
		-1
	};

	// Check for errors
	if (checker_sb_opts.new_stdout_fd < 0) {
		error_log("Failed to open '", tmp_dir.name(), "checker_out'",
			error(errno));

		// Close file descriptors
		sclose(sb_opts.new_stdout_fd);
		return JudgeResult(JudgeResult::JUDGE_ERROR, 0);
	}

	vector<string> exec_args, checker_args(4);

	// Initialise judge reports
	struct JudgeTestsReport {
		string tests, comments;
		JudgeResult::Status status;
	};

	JudgeTestsReport initial = {
		"<table class=\"table\">\n"
			"<thead>\n"
				"<tr>"
					"<th class=\"test\">Test</th>"
					"<th class=\"result\">Result</th>"
					"<th class=\"time\">Time [s]</th>"
					"<th class=\"points\">Points</th>"
				"</tr>\n"
			"</thead>\n"
			"<tbody>\n",
		"",
		JudgeResult::OK
	};

	JudgeTestsReport final = initial;
	long long total_score = 0;

	// Run judge on tests
	for (auto& group : pconf.test_groups) {
		double ratio = 1.0; // group ratio
		JudgeTestsReport &judge_test_report = (group.points == 0 ?
			initial : final);

		vector<TestResult> group_result;

		for (auto& test : group.tests) {
			sb_opts.time_limit = test.time_limit;

			// Reopen sb_opts.new_stdin
			// Close sb_opts.new_stdin_fd
			if (sb_opts.new_stdin_fd >= 0)
				sclose(sb_opts.new_stdin_fd);

			// Open sb_opts.new_stdin_fd
			if ((sb_opts.new_stdin_fd = open(
					concat(package_path, "tests/", test.name, ".in").c_str(),
					O_RDONLY | O_LARGEFILE | O_NOFOLLOW)) == -1) {
				error_log("Failed to open: '", package_path, "tests/",
					test.name, ".in'", error(errno));

				// Close file descriptors
				sclose(sb_opts.new_stdout_fd);
				sclose(checker_sb_opts.new_stdout_fd);
				return JudgeResult(JudgeResult::JUDGE_ERROR, 0);
			}

			// Truncate sb_opts.new_stdout
			ftruncate(sb_opts.new_stdout_fd, 0);
			lseek(sb_opts.new_stdout_fd, 0, SEEK_SET);

			auto tmplog = stdlog();
			if (VERBOSITY > 1)
				tmplog("  ", widedString(test.name, 11, LEFT), " ");

			// Run
			sandbox::ExitStat es = sandbox::run(concat(tmp_dir.name(), "exec"),
				exec_args, &sb_opts);

			if (isPrefix(es.message, "failed to get syscall") ||
					isPrefix(es.message, "forbidden syscall"))
				error_log("Submission ", submission_id, " (problem ", problem_id,
					"): ", test.name, ": ", es.message);

			// Update ratio
			ratio = std::min(ratio, 2.0 - 2.0 * (es.runtime / 10000) /
				(test.time_limit / 10000));

			// Construct Report
			group_result.push_back((TestResult){ &test.name, TestResult::ERROR,
				concat(usecToSecStr(es.runtime, 2, false), '/',
					usecToSecStr(test.time_limit, 2, false)) });

			// OK
			if (es.code == 0)
				group_result.back().status = TestResult::OK;

			// Runtime error
			else if (es.runtime < test.time_limit) {
				group_result.back().status = TestResult::RTE;
				if (judge_test_report.status != JudgeResult::JUDGE_ERROR)
					judge_test_report.status = JudgeResult::ERROR;
				ratio = 0.0;

				// Add test comment
				append(judge_test_report.comments)
					<< "<li><span class=\"test-id\">"
					<< htmlSpecialChars(test.name)<< "</span>"
					"Runtime error";

				if (es.message.size())
					append(judge_test_report.comments) << " (" << es.message
						<< ")";

				judge_test_report.comments += "</li>\n";

			// Time limit exceeded
			} else {
				group_result.back().status = TestResult::TLE;
				if (judge_test_report.status != JudgeResult::JUDGE_ERROR)
					judge_test_report.status = JudgeResult::ERROR;
				ratio = 0.0;

				// Add test comment
				append(judge_test_report.comments)
					<< "<li><span class=\"test-id\">"
					<< htmlSpecialChars(test.name) << "</span>"
					"Time limit exceeded</li>\n";
			}

			if (VERBOSITY > 1) {
				tmplog(widedString(usecToSecStr(es.runtime, 2, false), 4),
					" / ",
					widedString(usecToSecStr(test.time_limit, 2, false), 4),
					"    Status: ");

				if (es.code == 0)
					tmplog("\e[1;32mOK\e[m");
				else if (es.runtime < test.time_limit)
					tmplog("\e[1;33mRTE\e[m (", es.message, ")");
				else
					tmplog("\e[1;33mTLE\e[m");

				tmplog("   Exited with ", toString(es.code), " [ ",
					usecToSecStr(es.runtime, 6, false), " ]");
			}

			// If execution was correct, validate output
			if (group_result.back().status == TestResult::OK) {
				// Validate output
				checker_args[1] = concat(package_path, "tests/", test.name, ".in");
				checker_args[2] = concat(package_path, "tests/", test.name, ".out");
				checker_args[3] = concat(tmp_dir.sname(), "answer");

				if (VERBOSITY > 1)
					tmplog("  Output validation... ");

				// Truncate checker_sb_opts.new_stdout
				ftruncate(checker_sb_opts.new_stdout_fd, 0);
				lseek(checker_sb_opts.new_stdout_fd, 0, SEEK_SET);

				es = sandbox::run(concat(tmp_dir.sname(), "checker"),
					checker_args, &checker_sb_opts, sandbox::CheckerCallback(
						vector<string>(checker_args.begin() + 1,
							checker_args.end())));

				if (isPrefix(es.message, "failed to get syscall") ||
						isPrefix(es.message, "forbidden syscall"))
					error_log("Submission ", submission_id, " (problem ",
						problem_id, "): ", test.name, " - checker: ",
						es.message);

				if (es.code == 0) {
					if (VERBOSITY > 1)
						tmplog("\e[1;32mPASSED\e[m");

				// Wrong answer
				} else if (WIFEXITED(es.code) && WEXITSTATUS(es.code) == 1) {
					group_result.back().status = TestResult::WA;
					if (judge_test_report.status != JudgeResult::JUDGE_ERROR)
						judge_test_report.status = JudgeResult::ERROR;
					ratio = 0.0;

					if (VERBOSITY > 1)
						tmplog("\e[1;31mFAILED\e[m");

					// Get checker output
					char buff[204] = {};
					FILE *f = fopen(
						concat(tmp_dir.sname(), "checker_out").c_str(), "r");
					if (f != nullptr) {
						ssize_t ret = fread(buff, 1, 204, f);

						// Remove trailing white characters
						while (ret > 0 && isspace(buff[ret - 1]))
							buff[--ret] = '\0';

						if (ret > 200)
							strncpy(buff + 200, "...", 4);

						fclose(f);
					}

					// Add test comment
					append(judge_test_report.comments)
						<< "<li><span class=\"test-id\">"
						<< htmlSpecialChars(test.name) << "</span>"
						<< htmlSpecialChars(buff) << "</li>\n";

					if (VERBOSITY > 1)
						tmplog(" \"", buff, "\"");

				} else if (es.runtime < test.time_limit) {
					group_result.back().status = TestResult::CHECKER_ERROR;
					judge_test_report.status = JudgeResult::JUDGE_ERROR;
					ratio = 0.0;

					// Add test comment
					append(judge_test_report.comments)
						<< "<li><span class=\"test-id\">"
						<< htmlSpecialChars(test.name)<< "</span>"
						"Checker runtime error";

					if (es.message.size())
						append(judge_test_report.comments) << " (" << es.message
							<< ")";

					judge_test_report.comments += "</li>\n";

					if (VERBOSITY > 1)
						tmplog("\e[1;33mRTE\e[m (", es.message, ")");

				} else {
					group_result.back().status = TestResult::CHECKER_ERROR;
					judge_test_report.status = JudgeResult::JUDGE_ERROR;
					ratio = 0.0;

					// Add test comment
					append(judge_test_report.comments)
						<< "<li><span class=\"test-id\">"
						<< htmlSpecialChars(test.name) << "</span>"
						"Checker time limit exceeded</li>\n";

					if (VERBOSITY > 1)
						tmplog("\e[1;33mTLE\e[m");
				}

				if (VERBOSITY > 1)
					tmplog("   Exited with ", toString(es.code), " [ ",
						usecToSecStr(es.runtime, 6, false)," ]");
			}
		}

		// assert that group_result is not empty
		if (group_result.empty()) {
			error_log("Error: group_result is empty");
			// Close file descriptors
			if (sb_opts.new_stdin_fd)
				sclose(sb_opts.new_stdin_fd);
			sclose(sb_opts.new_stdout_fd);
			sclose(checker_sb_opts.new_stdout_fd);
			return JudgeResult(JudgeResult::JUDGE_ERROR, 0);
		}

		// Update score
		total_score += round(group.points * ratio);

		if (VERBOSITY > 1)
			stdlog("  Score: ",
				toString<long long>(round(group.points * ratio)), " / ",
				toString(group.points), " (ratio: ", toString(ratio, 3), ")");

		// Append first row
		append(judge_test_report.tests) << "<tr>"
				"<td>" << htmlSpecialChars(*group_result[0].name) << "</td>"
				<< TestResult::statusToTdString(group_result[0].status)
				<< "<td>" << group_result[0].time << "</td>"
				"<td class=\"groupscore\" rowspan=\""
					<< toString(group.tests.size()) << "\">"
					<< toString<long long>(round(group.points * ratio)) << "/"
					<< toString(group.points) << "</td>"
			"</tr>\n";

		for (vector<TestResult>::iterator i = ++group_result.begin(),
				end = group_result.end(); i != end; ++i)
			append(judge_test_report.tests) << "<tr>"
					"<td>" << htmlSpecialChars(*i->name) << "</td>"
					<< TestResult::statusToTdString(i->status)
					<< "<td>" << i->time << "</td>"
				"</tr>\n";
	}

	if (VERBOSITY > 1)
		stdlog("Total score: ", toString(total_score));

	// Complete reports
	initial.tests.append("</tbody>\n"
		"</table>\n");
	final.tests.append("</tbody>\n"
		"</table>\n");

	// Close file descriptors
	if (sb_opts.new_stdin_fd)
		sclose(sb_opts.new_stdin_fd);
	sclose(sb_opts.new_stdout_fd);
	sclose(checker_sb_opts.new_stdout_fd);

	return JudgeResult(initial.status, total_score,
		initial.tests + (initial.comments.empty() ?	""
			: concat("<ul class=\"test-comments\">", initial.comments,
				"</ul>\n")),
		final.tests + (final.comments.empty() ? ""
			: concat("<ul class=\"test-comments\">", final.comments,
				"</ul>\n")));
}
