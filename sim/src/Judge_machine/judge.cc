#include "judge.h"
#include "main.h"

#include <simlib/compile.h>
#include <simlib/sandbox_checker_callback.h>
#include <simlib/sim/simfile.h>
#include <simlib/utilities.h>

using std::string;
using std::vector;

constexpr size_t COMPILE_ERRORS_MAX_LENGTH = 100 << 10; // 100 KiB

namespace {

struct TestResult {
	const string* name;
	enum Status : uint8_t {
		OK,
		WA, // Wrong answer
		TLE, // Time limit exceeded
		MLE, // Memory limit exceeded
		RTE, // Runtime error
		ERROR,
		CHECKER_ERROR,
		SYSTEM_ERROR
	} status;
	string time;

	TestResult(const string* n, Status s, const string& t)
		: name(n), status(s), time(t) {}

	static string statusToTdString(Status s) {
		switch (s) {
		case OK:
			return "<td class=\"ok\">OK</td>";
		case WA:
			return "<td class=\"wa\">Wrong answer</td>";
		case TLE:
			return "<td class=\"tl-rte\">Time limit exceeded</td>";
		case MLE:
			return "<td class=\"tl-rte\">Memory limit exceeded</td>";
		case RTE:
			return "<td class=\"tl-rte\">Runtime error</td>";
		case ERROR:
			return "<td class=\"wa\">Error</td>";
		case CHECKER_ERROR:
			return "<td class=\"judge-error\">Checker error</td>";
		case SYSTEM_ERROR:
			return "<td class=\"judge-error\">System error</td>";
		}

		return "<td>Unknown</td>";
	}
};

} // anonymous namespace

JudgeResult judge(string submission_id, string problem_id) {
	string package_path = concat("problems/", problem_id, '/');

	// Load config
	sim::Simfile pconf;
	try {
		if (VERBOSITY > 1)
			stdlog("Validating config.conf...");
		pconf.loadFrom(package_path);
		if (VERBOSITY > 1)
			stdlog("Validation passed.");

	} catch (std::exception& e) {
		if (VERBOSITY > 0)
			errlog("Error: ", e.what());
		return JudgeResult(JudgeResult::JUDGE_ERROR, 0);
	}

	// Compile solution
	try {
		string compile_errors;
		if (0 != compile(concat("solutions/", submission_id, ".cpp"),
			concat(tmp_dir.sname(), "exec"), (VERBOSITY >> 1) + 1,
			30 * 1000000, // 30 s
			&compile_errors, COMPILE_ERRORS_MAX_LENGTH, "./proot"))
		{
			return JudgeResult(JudgeResult::COMPILE_ERROR, 0, concat(
				"<h2>Compilation failed</h2>"
				"<pre class=\"compile-errors\">",
				htmlSpecialChars(compile_errors), "</pre>"));
		}

		// Compile checker
		if (0 != compile(concat(package_path, "check/", pconf.checker),
			concat(tmp_dir.sname(), "checker"), (VERBOSITY >> 1) + 1,
			30 * 1000000, // 30 s
			nullptr, 0, "./proot"))
		{
			return JudgeResult(JudgeResult::COMPILE_ERROR, 0,
				"<h2>Compilation failed</h2>"
				"<pre class=\"compile-errors\">"
					"Checker compilation error"
				"</pre>");
		}

	} catch (const std::exception& e) {
		stdlog("Compilation error: ", e.what());
		errlog("Compilation error: ", e.what());
		return JudgeResult(JudgeResult::JUDGE_ERROR, 0,
			"<h2>Compilation failed</h2>"
			"<pre class=\"compile-errors\">"
				"Judge machine error"
			"</pre>");
	}

	// Prepare runtime environment
	Sandbox::Options sb_opts = {
		-1,
		open(concat(tmp_dir.sname(), "answer").c_str(),
			O_WRONLY | O_CREAT | O_TRUNC, S_0644),
		-1,
		0, // Will be set separately for each test later
		pconf.memory_limit << 10
	};

	// Check for errors
	if (sb_opts.new_stdout_fd < 0) {
		errlog("Failed to open '", tmp_dir.name(), "answer'", error(errno));
		return JudgeResult(JudgeResult::JUDGE_ERROR, 0);
	}

	Sandbox::Options checker_sb_opts = {
		-1,
		getUnlinkedTmpFile(),
		-1,
		10 * 1000000, // 10 s
		256 << 20 // 256 MiB
	};

	// Check for errors
	if (checker_sb_opts.new_stdout_fd < 0) {
		errlog("Failed to create file for checker output ", error(errno));

		// Close file descriptors
		sclose(sb_opts.new_stdout_fd);
		return JudgeResult(JudgeResult::JUDGE_ERROR, 0);
	}

	vector<string> checker_args(4);

	struct JudgeTestsReport {
		string tests, comments;
		JudgeResult::Status status = JudgeResult::OK;

		explicit JudgeTestsReport(const char* header) : tests(
			concat(header, "<table class=\"table\">\n"
				"<thead>\n"
					"<tr>"
						"<th class=\"test\">Test</th>"
						"<th class=\"result\">Result</th>"
						"<th class=\"time\">Time [s]</th>"
						"<th class=\"points\">Points</th>"
					"</tr>\n"
				"</thead>\n"
				"<tbody>\n")) {}
	};

	// Initialise judge reports
	JudgeTestsReport initial("<h2>Initial testing report</h2>");
	JudgeTestsReport final("<h2>Final testing report</h2>");
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
				O_RDONLY | O_LARGEFILE | O_NOFOLLOW)) == -1)
			{
				errlog("Failed to open: '", package_path, "tests/", test.name,
					".in'", error(errno));

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
			Sandbox::ExitStat es;
			try {
				es = Sandbox::run(concat(tmp_dir.name(), "exec"), {}, sb_opts);
			} catch (const std::exception& e) {
				ERRLOG_CAUGHT(e);

				// System error
				ratio = 0.0;
				judge_test_report.status = JudgeResult::JUDGE_ERROR;

				// Construct Report
				group_result.emplace_back(&test.name, TestResult::SYSTEM_ERROR,
					concat("-.-- / ", usecToSecStr(test.time_limit, 2, false)));

				if (VERBOSITY > 1)
					tmplog("-.-- / ",
						widedString(usecToSecStr(test.time_limit, 2, false), 4),
						"    Status: \033[1;34mSYSTEM ERROR\033[m (",
						e.what(), ")");

				continue;
			}

			if (isPrefixIn(es.message, {"Error: ", "failed to get syscall",
				"forbidden syscall"}))
			{
				errlog("Submission ", submission_id, " (problem ", problem_id,
					"): ", test.name, " -> ", es.message);
			}

			// Update ratio
			ratio = std::min(ratio, 2.0 - 2.0 * (es.runtime / 10000) /
				(test.time_limit / 10000));

			// Construct Report
			group_result.emplace_back(&test.name, TestResult::ERROR,
				concat(usecToSecStr(es.runtime, 2, false), " / ",
					usecToSecStr(test.time_limit, 2, false)));

			// OK
			if (es.code == 0) {
				group_result.back().status = TestResult::OK;

			// Time limit exceeded
			} else if (es.runtime >= test.time_limit) {
				ratio = 0.0;
				group_result.back().status = TestResult::TLE;
				if (judge_test_report.status != JudgeResult::JUDGE_ERROR)
					judge_test_report.status = JudgeResult::ERROR;

				// Add test comment
				back_insert(judge_test_report.comments,
					"<li>"
						"<span class=\"test-id\">", htmlSpecialChars(test.name),
						"</span>"
						"Time limit exceeded"
					"</li>\n");

			// Memory limit exceeded
			} else if (es.message == "Memory limit exceeded") {
				ratio = 0.0;
				group_result.back().status = TestResult::MLE;
				if (judge_test_report.status != JudgeResult::JUDGE_ERROR)
					judge_test_report.status = JudgeResult::ERROR;

				// Add test comment
				back_insert(judge_test_report.comments,
					"<li>"
						"<span class=\"test-id\">", htmlSpecialChars(test.name),
						"</span>"
						"Memory limit exceeded"
					"</li>\n");

			// Runtime error
			} else {
				ratio = 0.0;
				group_result.back().status = TestResult::RTE;
				if (judge_test_report.status != JudgeResult::JUDGE_ERROR)
					judge_test_report.status = JudgeResult::ERROR;

				// Add test comment
				back_insert(judge_test_report.comments,
					"<li>"
						"<span class=\"test-id\">", htmlSpecialChars(test.name),
						"</span>"
						"Runtime error");

				if (es.message.size())
					back_insert(judge_test_report.comments, " (", es.message,
						')');

				judge_test_report.comments += "</li>\n";
			}


			if (VERBOSITY > 1) {
				tmplog(widedString(usecToSecStr(es.runtime, 2, false), 4),
					" / ",
					widedString(usecToSecStr(test.time_limit, 2, false), 4),
					"    Status: ");

				if (group_result.back().status == TestResult::OK)
					tmplog("\033[1;32mOK\033[m");
				else if (group_result.back().status == TestResult::TLE)
					tmplog("\033[1;33mTLE\033[m");
				else if (group_result.back().status == TestResult::MLE)
					tmplog("\033[1;33mMLE\033[m");
				else if (group_result.back().status == TestResult::RTE)
					tmplog("\033[1;33mRTE\033[m (", es.message, ")");

				tmplog("   Exited with ", toString(es.code), " [ ",
					usecToSecStr(es.runtime, 6, false), " ]");
			}

			// If execution was correct, validate output
			if (group_result.back().status != TestResult::OK)
				continue;

			// Validate output
			checker_args[1] = concat(package_path, "tests/", test.name, ".in");
			checker_args[2] = concat(package_path, "tests/", test.name, ".out");
			checker_args[3] = concat(tmp_dir.sname(), "answer");

			if (VERBOSITY > 1)
				tmplog("  Checker: ");

			// Truncate checker_sb_opts.new_stdout
			ftruncate(checker_sb_opts.new_stdout_fd, 0);
			lseek(checker_sb_opts.new_stdout_fd, 0, SEEK_SET);

			try {
				es = Sandbox::run(concat(tmp_dir.sname(), "checker"),
					checker_args, checker_sb_opts, ".", CheckerCallback(
						{checker_args.begin() + 1, checker_args.end()}));
			} catch (const std::exception& e) {
				ERRLOG_CAUGHT(e);

				// System error
				ratio = 0.0;
				group_result.back().status = TestResult::SYSTEM_ERROR;
				judge_test_report.status = JudgeResult::JUDGE_ERROR;

				if (VERBOSITY > 1)
					tmplog("\033[1;34mSYSTEM ERROR\033[m (", e.what(), ")");
				continue;
			}

			if (isPrefixIn(es.message, {"Error: ", "failed to get syscall",
				"forbidden syscall"}))
			{
				errlog("Submission ", submission_id, " (problem ", problem_id,
					"): ", test.name, " (checker) -> ", es.message);
			}

			// OK
			if (es.code == 0) {
				// Test status is already set to TestResult::OK
				if (VERBOSITY > 1)
					tmplog("\033[1;32mOK\033[m");

			// Wrong answer
			} else if (WIFEXITED(es.code) && WEXITSTATUS(es.code) == 1) {
				ratio = 0.0;
				group_result.back().status = TestResult::WA;
				if (judge_test_report.status != JudgeResult::JUDGE_ERROR)
					judge_test_report.status = JudgeResult::ERROR;

				if (VERBOSITY > 1)
					tmplog("\033[1;31mWRONG\033[m");

				// Get checker output
				string checker_output;
				try {
					checker_output = sim::obtainCheckerOutput(
						checker_sb_opts.new_stdout_fd, 256);
				} catch (const std::exception& e) {
					errlog("Failed to obtain checker output -> ", e.what());
				}

				// Add test comment
				back_insert(judge_test_report.comments,
					"<li><span class=\"test-id\">",
					htmlSpecialChars(test.name), "</span>",
					htmlSpecialChars(checker_output), "</li>\n");

				if (VERBOSITY > 1)
					tmplog(" \"", checker_output, "\"");

			// Time limit exceeded
			} else if (es.runtime >= test.time_limit) {
				ratio = 0.0;
				group_result.back().status = TestResult::CHECKER_ERROR;
				judge_test_report.status = JudgeResult::JUDGE_ERROR;

				// Add test comment
				back_insert(judge_test_report.comments,
					"<li>"
						"<span class=\"test-id\">", htmlSpecialChars(test.name),
						"</span>"
						"Checker time limit exceeded"
					"</li>\n");

				if (VERBOSITY > 1)
					tmplog("\033[1;33mTLE\033[m");

			// Memory limit exceeded
			} else if (es.message == "Memory limit exceeded") {
				ratio = 0.0;
				group_result.back().status = TestResult::CHECKER_ERROR;
				judge_test_report.status = JudgeResult::JUDGE_ERROR;

				// Add test comment
				back_insert(judge_test_report.comments,
					"<li>"
						"<span class=\"test-id\">", htmlSpecialChars(test.name),
						"</span>"
						"Checker memory limit exceeded"
					"</li>\n");

				if (VERBOSITY > 1)
					tmplog("\033[1;33mMLE\033[m");

			} else {
				ratio = 0.0;
				group_result.back().status = TestResult::CHECKER_ERROR;
				judge_test_report.status = JudgeResult::JUDGE_ERROR;

				// Add test comment
				back_insert(judge_test_report.comments,
					"<li>"
						"<span class=\"test-id\">", htmlSpecialChars(test.name),
						"</span>"
						"Checker runtime error");

				if (es.message.size())
					back_insert(judge_test_report.comments, " (", es.message,
						')');

				judge_test_report.comments += "</li>\n";

				if (VERBOSITY > 1)
					tmplog("\033[1;33mRTE\033[m (", es.message, ")");
			}

			if (VERBOSITY > 1)
				tmplog("   Exited with ", toString(es.code), " [ ",
					usecToSecStr(es.runtime, 6, false)," ]");
		}

		// assert that group_result is not empty
		if (group_result.empty()) {
			errlog("Error: group_result is empty");
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
		back_insert(judge_test_report.tests, "<tr>"
				"<td>", htmlSpecialChars(*group_result[0].name), "</td>",
				TestResult::statusToTdString(group_result[0].status),
				"<td>", group_result[0].time, "</td>"
				"<td class=\"groupscore\" rowspan=\"",
					toString(group.tests.size()), "\">",
					toString<long long>(round(group.points * ratio)), " / ",
					toString(group.points), "</td>"
			"</tr>\n");

		for (auto it = ++group_result.begin(), end = group_result.end();
			it != end; ++it)
		{
			back_insert(judge_test_report.tests, "<tr>"
					"<td>", htmlSpecialChars(*it->name), "</td>",
					TestResult::statusToTdString(it->status),
					"<td>", it->time, "</td>"
				"</tr>\n");
		}
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
