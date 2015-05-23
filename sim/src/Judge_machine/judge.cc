#include "judge.h"
#include "main.h"

#include "../include/debug.h"
#include "../include/process.h"
#include "../include/sandbox.h"
#include "../include/sandbox_checker_callback.h"
#include "../include/string.h"
#include "../include/utility.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

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

static int compile(const string& source, const string& exec, string* c_errors,
		size_t c_errors_max_len = -1) {
	FILE *cef = fopen(c_errors == NULL ? "/dev/null" :
			(tmp_dir->sname() + "compile_errors").c_str(), "w");
	if (cef == NULL) {
		eprintf("Failed to open 'compile_errors' - %s\n", strerror(errno));
		return 1;
	}

	if (VERBOSE)
		printf("Compiling: '%s' ", (source).c_str());

	// Run compiler
	vector<string> args;
	append(args)("g++")("-O2")("-static")("-lm")(source)("-o")(exec);

	/* Compile as 32 bit executable (not essential but if checker/judge-machine
	*  will be x86_63 and Conver i386 then checker won't work, with it its more
	*  secure (see making i386 syscall from x86_64))
	*/
	append(args)("-m32");

	if (VERBOSE) {
		printf("(Command: '%s", args[0].c_str());
		for (size_t i = 1, end = args.size(); i < end; ++i)
			printf(" %s", args[i].c_str());
		printf("')\n");
	}

	spawn_opts sopts = { NULL, NULL, cef };
	int compile_status = spawn(args[0], args, &sopts);

	// Check for errors
	if (compile_status != 0) {
		if (VERBOSE)
			eprintf("Failed.\n");

		if (c_errors)
			*c_errors = getFileContents(tmp_dir->sname() + "compile_errors", 0,
				c_errors_max_len);

		fclose(cef);
		return 2;

	} else if (VERBOSE)
		printf("Completed successfully.\n");

	if (c_errors)
		*c_errors = "";

	fclose(cef);
	return 0;
}

namespace {

/**
 * @brief ProblemConfig holds SIM package config
 * @details Holds problem name, problem tag, problem statement, checker,
 * solution, memory limit and grouped tests with time limit for each
 */
class ProblemConfig {
public:
	std::string name, tag, statement, checker, solution;
	unsigned long long memory_limit; // in kB

	/**
	 * @brief Holds test
	 */
	struct Test {
		std::string name;
		unsigned long long time_limit; // in usec
	};

	/**
	 * @brief Holds group of tests
	 * @details Holds group of tests and points for them
	 */
	struct Group {
		std::vector<Test> tests;
		long long points;
	};

	std::vector<Group> test_groups;

	ProblemConfig() : name(), tag(), statement(), checker(), solution(),
		memory_limit(0), test_groups() {}

	~ProblemConfig() {}

	/**
	 * @brief loads and validates config file from problem package
	 * @p package_path
	 * @details Validates problem config (memory limits, tests existence etc.)
	 *
	 * @param package_path Path to problem package directory
	 * @return 0 on success, -1 on error
	 */
	int loadConfig(string package_path);
};

} // anonymous namespace

int ProblemConfig::loadConfig(string package_path) {
	// Add slash to package_path
	if (package_path.size() > 0 && *--package_path.end() != '/')
		package_path += '/';

	vector<string> f = getFileByLines(package_path + "conf.cfg",
		GFBL_IGNORE_NEW_LINES);

	if (VERBOSE)
		printf("Validating conf.cfg...\n");

	// Problem name
	if (f.size() < 1) {
		eprintf("Error: conf.cfg: Missing problem name\n");
		return -1;
	}

	// Problem tag
	if (f.size() < 2) {
		eprintf("Error: conf.cfg: Missing problem tag\n");
		return -1;
	}

	if (f[1].size() > 4) {
		eprintf("Error: conf.cfg: Problem tag too long (max 4 characters)\n");
		return -1;
	}

	// Statement
	if (f.size() < 3 || f[2].empty())
		eprintf("Warning: conf.cfg: Missing statement\n");

	else if (f[2].find('/') != string::npos ||
			access(package_path + "doc/" + f[2], F_OK) == -1) {
		eprintf("Error: conf.cfg: Invalid statement: 'doc/%s' - %s\n",
			f[2].c_str(), strerror(errno));
		return -1;
	}

	// Checker
	if (f.size() < 4) {
		eprintf("Error: conf.cfg: Missing checker\n");
		return -1;
	}

	if (f[3].find('/') != string::npos ||
			access(package_path + "check/" + f[3], F_OK) == -1) {
		eprintf("Error: conf.cfg: Invalid checker: 'check/%s' - %s\n",
			f[3].c_str(), strerror(errno));
		return -1;
	}

	// Solution
	if (f.size() < 5 || f[4].empty())
		eprintf("Warning: conf.cfg: Missing solution\n");

	else if (f[4].find('/') != string::npos ||
			access(package_path + "prog/" + f[4], F_OK) == -1) {
		eprintf("Error: conf.cfg: Invalid solution: 'prog/%s' - %s\n",
			f[4].c_str(), strerror(errno));
		return -1;
	}

	// Memory limit (in kB)
	if (f.size() < 6) {
		eprintf("Error: conf.cfg: Missing memory limit\n");
		return -1;
	}

	if (strtou(f[5], &memory_limit) == -1) {
		eprintf("Error: conf.cfg: Invalid memory limit: '%s'\n", f[5].c_str());
		return -1;
	}

	name = f[0];
	tag = f[1];
	statement = f[2];
	checker = f[3];
	solution = f[4];

	test_groups.clear();

	// Tests
	vector<string> line;
	for (int i = 6, flen = f.size(); i < flen; ++i) {
		size_t beg = 0, end = 0, len = f[i].size();
		line.clear();

		// Get line split on spaces and tabs
		do {
			end = beg + strcspn(f[i].c_str() + beg, " \t");
			line.push_back(string(f[i].begin() + beg, f[i].begin() + end));

			// Remove blank strings (created by multiple separators)
			if (line.back().empty())
				line.pop_back();

			beg = ++end;
		} while (end < len);

		// Ignore empty lines
		if (line.empty())
			continue;

		// Validate line
		if (line.size() != 2 && line.size() != 3) {
			eprintf("Error: conf.cfg: Tests - invalid line format (line %i)\n",
				i);
			return -1;
		}

		// Test name
		if (line[0].find('/') != string::npos ||
				access((package_path + "tests/" + line[0] + ".in"), F_OK) == -1) {
			eprintf("Error: conf.cfg: Invalid test: '%s' - %s\n",
				line[0].c_str(), strerror(errno));
			return -1;
		}

		ProblemConfig::Test test = { line[0], 0 };

		// Time limit
		if (!isReal(line[1])) {
			eprintf("Error: conf.cfg: Invalid time limit: '%s' (line %i)\n",
				line[1].c_str(), i);
			return -1;
		}

		test.time_limit = round(strtod(line[1].c_str(), NULL) * 1000000LL);

		// Points
		if (line.size() == 3) {
			test_groups.push_back(ProblemConfig::Group());

			if (strtoi(line[2], &test_groups.back().points) == -1) {
				eprintf("Error: conf.cfg: Invalid points for group: '%s' "
					"(line %i)\n", line[2].c_str(), i);
				return -1;
			}
		}

		test_groups.back().tests.push_back(test);
	}

	if (VERBOSE)
		printf("Validation passed.\n");

	return 0;
}

namespace {

struct TestResult {
	const string* name;
	enum Status { ERROR, OK, WA, TLE, RTE } status;
	string time;

	static string statusToTdString(Status s) {
		switch (s) {
		case ERROR:
			return "<td class=\"wa\">Error</td>";
		case OK:
			return "<td class=\"ok\">OK</td>";
		case WA:
			return "<td class=\"wa\">Wrong answer</td>";
		case TLE:
			return "<td class=\"tl-re\">Time limit exceeded</td>";
		case RTE:
			return "<td class=\"tl-re\">Runtime error</td>";
		}

		return "<td>Unknown</td>";
	}
};

} // anonymous namespace

JudgeResult judge(string submission_id, string problem_id) {
	string package_path = "problems/" + problem_id + "/";

	// Load config
	ProblemConfig pconf;
	if (pconf.loadConfig(package_path) == -1)
		return (JudgeResult){ JudgeResult::ERROR, 0, "" };

	// Compile solution
	string compile_errors;
	if (0 != compile("solutions/" + submission_id + ".cpp",
			tmp_dir->sname() + "exec", &compile_errors,
			COMPILE_ERRORS_MAX_LENGTH))
		return (JudgeResult){ JudgeResult::CERROR, 0,
			convertString(htmlSpecialChars(compile_errors)) };

	// Compile checker
	if (0 != compile(package_path + "check/" + pconf.checker,
			tmp_dir->sname() + "checker", NULL))
		return (JudgeResult){ JudgeResult::CERROR, 0,
			"Checker compilation error" };

	// Prepare runtime environment
	sandbox::options sb_opt = {
		0, // Will be set separately for each test
		pconf.memory_limit << 10,
		NULL,
		fopen((tmp_dir->sname() + "answer").c_str(), "w"),
		fopen("/dev/null", "w")
	};

	sandbox::options check_sb_opt = {
		10000000ull, // 10s
		256 << 20, // 256 MB
		fopen("/dev/null", "r"),
		fopen((tmp_dir->sname() + "checker_out").c_str(), "w"),
		sb_opt.new_stderr
	};

	// Check for errors
	if (sb_opt.new_stderr == NULL) {
		eprintf("Failed to open '/dev/null' - %s\n", strerror(errno));
		return (JudgeResult){ JudgeResult::ERROR, 0, "" };
	}

	if (check_sb_opt.new_stdout == NULL) {
		eprintf("Failed to open '%sanswer' - %s\n", tmp_dir->name(),
			strerror(errno));
		fclose(sb_opt.new_stderr);
		return (JudgeResult){ JudgeResult::ERROR, 0, "" };
	}

	if (check_sb_opt.new_stdin == NULL) {
		eprintf("Failed to open '/dev/null' - %s\n", strerror(errno));
		fclose(sb_opt.new_stderr);
		fclose(sb_opt.new_stdout);
		return (JudgeResult){ JudgeResult::ERROR, 0, "" };
	}

	if (check_sb_opt.new_stdout == NULL) {
		eprintf("Failed to open '%schecker_out' - %s\n", tmp_dir->name(),
			strerror(errno));
		fclose(sb_opt.new_stderr);
		fclose(sb_opt.new_stdout);
		fclose(check_sb_opt.new_stdin);
		return (JudgeResult){ JudgeResult::ERROR, 0, "" };
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
			sb_opt.time_limit = test->time_limit;

			// Reopening sb_opt.new_stdin
			if (sb_opt.new_stdin)
				fclose(sb_opt.new_stdin);

			if ((sb_opt.new_stdin = fopen((package_path + "tests/" +
					test->name + ".in").c_str(), "r")) == NULL) {
				eprintf("Failed to open: '%s' - %s\n",
					(package_path + "tests/" + test->name + ".in").c_str(),
					strerror(errno));
				return (JudgeResult){ JudgeResult::ERROR, 0, "" };
			}

			// Truncate sb_opt.new_stdout
			ftruncate(fileno(sb_opt.new_stdout), 0);
			rewind(sb_opt.new_stdout);

			if (VERBOSE) {
				printf("  %-11s ", test->name.c_str());
				fflush(stdout);
			}

			// Run
			sandbox::ExitStat es = sandbox::run(tmp_dir->sname() + "exec",
				exec_args, &sb_opt);

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
				judge_test_report.status = JudgeResult::ERROR;
				ratio = 0.0;

				// Add test comment
				append(judge_test_report.comments)
					<< "<li><span class=\"test-id\">"
					<< htmlSpecialChars(test->name) << "</span>"
					"Time limit exceeded</li>\n";
			}

			if (VERBOSE) {
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

				if (VERBOSE) {
					printf("  Output validation... ");
					fflush(stdout);
				}

				// Truncate check_sb_opt.new_stdout
				ftruncate(fileno(check_sb_opt.new_stdout), 0);
				rewind(check_sb_opt.new_stdout);

				es = sandbox::run(tmp_dir->sname() + "checker", checker_args,
					&check_sb_opt, sandbox::CheckerCallback(
						vector<string>(checker_args.begin() + 1,
							checker_args.end())));

				if (es.code == 0) {
					if (VERBOSE)
						printf("\e[1;32mPASSED\e[m");

				// Wrong answer
				} else if (WIFEXITED(es.code) && WEXITSTATUS(es.code) == 1) {
					group_result.back().status = TestResult::WA;
					judge_test_report.status = JudgeResult::ERROR;
					ratio = 0.0;

					if (VERBOSE)
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

					if (VERBOSE)
						printf(" \"%s\"", buff);

				} else if (es.runtime < test->time_limit) {
					if (VERBOSE)
						printf("\e[1;33mRTE\e[m (%s)", es.message.c_str());

				} else if (VERBOSE)
					printf("\e[1;33mTLE\e[m");

				if (VERBOSE)
					printf("   Exited with %i [ %s ]", es.code,
						usecToSecStr(es.runtime, 6, false).c_str());
			}

			if (VERBOSE)
				printf("\n");
		}

		// group_result cannot be empty but for sure...
		if (group_result.empty())
			return (JudgeResult){ JudgeResult::ERROR, 0, "" };

		// Update score
		total_score += round(group->points * ratio);

		if (VERBOSE)
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

	if (VERBOSE)
		printf("Total score: %lli\n", total_score);

	// Complete reports
	initial.tests.append("</tbody>\n"
		"</table>\n");
	final.tests.append("</tbody>\n"
		"</table>\n");

	// Close files
	if (sb_opt.new_stdin)
		fclose(sb_opt.new_stdin);

	fclose(sb_opt.new_stdout);
	fclose(sb_opt.new_stderr);
	fclose(check_sb_opt.new_stdin);
	fclose(check_sb_opt.new_stdout);

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
