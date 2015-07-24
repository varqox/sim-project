#include "judge.h"
#include "main.h"

#include "../include/compile.h"
#include "../include/debug.h"
#include "../include/sandbox.h"
#include "../include/sandbox_checker_callback.h"
#include "../include/sim_problem.h"
#include "../include/string.h"
#include "../include/utility.h"

#include <cerrno>
#include <cmath>
#include <cstring>

#define foreach(i,x) for (__typeof(x.begin()) i = x.begin(), \
	i ##__end = x.end(); i != i ##__end; ++i)

using std::string;
using std::vector;

const size_t COMPILE_ERRORS_MAX_LENGTH = 100 << 10; // 100kB

static string convertString(const string& str) {
	string res;
	foreach (i, str) {
		if (*i == '\\')
			res += "\\\\";
		else if (*i == '\n')
			res += "\\n";
		else
			res += *i;
	}

	return res;
}

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
			return "<td class=\"tl-rte\">Checker error</td>";
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
	string package_path = "problems/" + problem_id + "/";

	// Load config
	ProblemConfig pconf;
	if (pconf.loadConfig(package_path, (VERBOSITY >> 1) + 1) == -1)
		return (JudgeResult){ JudgeResult::JUDGE_ERROR, 0, "" };

	// Compile solution
	string compile_errors;
	if (0 != compile("solutions/" + submission_id + ".cpp",
			tmp_dir->sname() + "exec", (VERBOSITY >> 1) + 1, &compile_errors,
			COMPILE_ERRORS_MAX_LENGTH, "./proot"))
		return (JudgeResult){ JudgeResult::CERROR, 0,
			convertString(htmlSpecialChars(compile_errors)) };

	// Compile checker
	if (0 != compile(package_path + "check/" + pconf.checker,
			tmp_dir->sname() + "checker", (VERBOSITY >> 1) + 1, NULL, 0,
			"./proot"))
		return (JudgeResult){ JudgeResult::CERROR, 0,
			"Checker compilation error" };

	// Prepare runtime environment
	sandbox::options sb_opts = {
		0, // Will be set separately for each test later
		pconf.memory_limit << 10,
		-1,
		open((tmp_dir->sname() + "answer").c_str(),
			O_WRONLY | O_CREAT | O_TRUNC,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), // (mode: 0644/rw-r--r--)
		-1
	};

	// Check for errors
	if (sb_opts.new_stdout_fd < 0) {
		eprintf("Failed to open '%sanswer' - %s\n", tmp_dir->name(),
			strerror(errno));
		return (JudgeResult){ JudgeResult::JUDGE_ERROR, 0, "" };
	}

	sandbox::options checker_sb_opts = {
		10 * 1000000, // 10s
		256 << 20, // 256 MB
		-1,
		open((tmp_dir->sname() + "checker_out").c_str(),
			O_WRONLY | O_CREAT | O_TRUNC,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), // (mode: 0644/rw-r--r--)
		-1
	};

	// Check for errors
	if (checker_sb_opts.new_stdout_fd < 0) {
		eprintf("Failed to open '%schecker_out' - %s\n", tmp_dir->name(),
			strerror(errno));

		// Close file descriptors
		while (close(sb_opts.new_stdout_fd) == -1 && errno == EINTR) {}
		return (JudgeResult){ JudgeResult::JUDGE_ERROR, 0, "" };
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
	foreach (group, pconf.test_groups) {
		double ratio = 1.0;
		JudgeTestsReport &judge_test_report = group->points == 0 ?
			initial : final;

		vector<TestResult> group_result;

		foreach (test, group->tests) {
			sb_opts.time_limit = test->time_limit;

			// Reopen sb_opts.new_stdin
			// Close sb_opts.new_stdin_fd
			if (sb_opts.new_stdin_fd >= 0)
				while (close(sb_opts.new_stdin_fd) == -1 && errno == EINTR) {}

			// Open sb_opts.new_stdin_fd
			if ((sb_opts.new_stdin_fd = open(
					(package_path + "tests/" + test->name + ".in").c_str(),
					O_RDONLY | O_LARGEFILE | O_NOFOLLOW)) == -1) {
				eprintf("Failed to open: '%s' - %s\n",
					(package_path + "tests/" + test->name + ".in").c_str(),
					strerror(errno));

				// Close file descriptors
				while (close(sb_opts.new_stdout_fd) == -1 && errno == EINTR) {}
				while (close(checker_sb_opts.new_stdout_fd) == -1 &&
					errno == EINTR) {}
				return (JudgeResult){ JudgeResult::JUDGE_ERROR, 0, "" };
			}

			// Truncate sb_opts.new_stdout
			ftruncate(sb_opts.new_stdout_fd, 0);
			lseek(sb_opts.new_stdout_fd, 0, SEEK_SET);

			if (VERBOSITY > 1) {
				printf("  %-11s ", test->name.c_str());
				fflush(stdout);
			}

			// Run
			sandbox::ExitStat es = sandbox::run(tmp_dir->sname() + "exec",
				exec_args, &sb_opts);

			// Update ratio
			ratio = std::min(ratio, 2.0 - 2.0 * (es.runtime / 10000) /
				(test->time_limit / 10000));

			// Construct Report
			group_result.push_back((TestResult){ &test->name, TestResult::ERROR,
				usecToSecStr(es.runtime, 2, false) + "/" +
					usecToSecStr(test->time_limit, 2, false) });

			// OK
			if (es.code == 0)
				group_result.back().status = TestResult::OK;

			// Runtime error
			else if (es.runtime < test->time_limit) {
				group_result.back().status = TestResult::RTE;
				if (judge_test_report.status != JudgeResult::JUDGE_ERROR)
					judge_test_report.status = JudgeResult::ERROR;
				ratio = 0.0;

				// Add test comment
				append(judge_test_report.comments)
					<< "<li><span class=\"test-id\">"
					<< htmlSpecialChars(test->name)<< "</span>"
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
					<< htmlSpecialChars(test->name) << "</span>"
					"Time limit exceeded</li>\n";
			}

			if (VERBOSITY > 1) {
				printf("%4s / %-4s    Status: ",
					usecToSecStr(es.runtime, 2, false).c_str(),
					usecToSecStr(test->time_limit, 2, false).c_str());

				if (es.code == 0)
					printf("\e[1;32mOK\e[m");
				else if (es.runtime < test->time_limit)
					printf("\e[1;33mRTE\e[m (%s)", es.message.c_str());
				else
					printf("\e[1;33mTLE\e[m");

				printf("   Exited with %i [ %s ]", es.code,
					usecToSecStr(es.runtime, 6, false).c_str());
			}

			// If execution was correct, validate output
			if (group_result.back().status == TestResult::OK) {
				// Validate output
				checker_args[1] = package_path + "tests/" + test->name + ".in";
				checker_args[2] = package_path + "tests/" + test->name + ".out";
				checker_args[3] = tmp_dir->sname() + "answer";

				if (VERBOSITY > 1) {
					printf("  Output validation... ");
					fflush(stdout);
				}

				// Truncate checker_sb_opts.new_stdout
				ftruncate(checker_sb_opts.new_stdout_fd, 0);
				lseek(checker_sb_opts.new_stdout_fd, 0, SEEK_SET);

				es = sandbox::run(tmp_dir->sname() + "checker", checker_args,
					&checker_sb_opts, sandbox::CheckerCallback(
						vector<string>(checker_args.begin() + 1,
							checker_args.end())));

				if (es.code == 0) {
					if (VERBOSITY > 1)
						printf("\e[1;32mPASSED\e[m");

				// Wrong answer
				} else if (WIFEXITED(es.code) && WEXITSTATUS(es.code) == 1) {
					group_result.back().status = TestResult::WA;
					if (judge_test_report.status != JudgeResult::JUDGE_ERROR)
						judge_test_report.status = JudgeResult::ERROR;
					ratio = 0.0;

					if (VERBOSITY > 1)
						printf("\e[1;31mFAILED\e[m");

					// Get checker output
					char buff[204] = {};
					FILE *f = fopen((tmp_dir->sname() + "checker_out").c_str(),
						"r");
					if (f != NULL) {
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
						<< htmlSpecialChars(test->name) << "</span>"
						<< htmlSpecialChars(buff) << "</li>\n";

					if (VERBOSITY > 1)
						printf(" \"%s\"", buff);

				} else if (es.runtime < test->time_limit) {
					group_result.back().status = TestResult::CHECKER_ERROR;
					judge_test_report.status = JudgeResult::JUDGE_ERROR;
					ratio = 0.0;

					if (VERBOSITY > 1)
						printf("\e[1;33mRTE\e[m (%s)", es.message.c_str());

				} else {
					group_result.back().status = TestResult::CHECKER_ERROR;
					judge_test_report.status = JudgeResult::JUDGE_ERROR;
					ratio = 0.0;

					if (VERBOSITY > 1)
						printf("\e[1;33mTLE\e[m");
				}

				if (VERBOSITY > 1)
					printf("   Exited with %i [ %s ]", es.code,
						usecToSecStr(es.runtime, 6, false).c_str());
			}

			if (VERBOSITY > 1)
				printf("\n");
		}

		// assert that group_result is not empty
		if (group_result.empty()) {
			// Close file descriptors
			if (sb_opts.new_stdin_fd)
				while (close(sb_opts.new_stdin_fd) == -1 && errno == EINTR) {}
			while (close(sb_opts.new_stdout_fd) == -1 && errno == EINTR) {}
			while (close(checker_sb_opts.new_stdout_fd) == -1 && errno == EINTR)
				{}
			return (JudgeResult){ JudgeResult::JUDGE_ERROR, 0, "" };
		}

		// Update score
		total_score += round(group->points * ratio);

		if (VERBOSITY > 1)
			printf("  Score: %lli / %lli (ratio: %.3lf)\n\n",
				(long long)round(group->points * ratio), group->points, ratio);

		// Append first row
		append(judge_test_report.tests) << "<tr>"
				"<td>" << htmlSpecialChars(*group_result[0].name) << "</td>"
				<< TestResult::statusToTdString(group_result[0].status)
				<< "<td>" << group_result[0].time << "</td>"
				"<td class=\"groupscore\" rowspan=\""
					<< toString(group->tests.size()) << "\">"
					<< toString((long long)round(group->points * ratio)) << "/"
					<< toString(group->points) << "</td>"
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
		printf("Total score: %lli\n", total_score);

	// Complete reports
	initial.tests.append("</tbody>\n"
		"</table>\n");
	final.tests.append("</tbody>\n"
		"</table>\n");

	// Close file descriptors
	if (sb_opts.new_stdin_fd)
		while (close(sb_opts.new_stdin_fd) == -1 && errno == EINTR) {}
	while (close(sb_opts.new_stdout_fd) == -1 && errno == EINTR) {}
	while (close(checker_sb_opts.new_stdout_fd) == -1 && errno == EINTR) {}

	return (JudgeResult){
		initial.status,
		total_score,
		convertString(final.tests + (final.comments.empty() ? ""
				: "<ul class=\"test-comments\">" + final.comments + "</ul>\n"))
			+ "\n" + convertString(initial.tests + (initial.comments.empty() ?
				"" : "<ul class=\"test-comments\">" + initial.comments +
					"</ul>\n"))
	};
}
