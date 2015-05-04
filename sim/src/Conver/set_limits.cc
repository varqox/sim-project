#include "convert_package.h"
#include "set_limits.h"

#include "../include/debug.h"
#include "../include/process.h"
#include "../include/sandbox.h"
#include "../include/sandbox_checker_callback.h"
#include "../include/utility.h"

#include <cassert>
#include <cerrno>
#include <cstring>

#define foreach(i,x) for (__typeof(x.begin()) i = x.begin(), \
	i ##__end = x.end(); i != i ##__end; ++i)

using std::pair;
using std::string;
using std::vector;

static int compile(const string& in, const string& out) {
	FILE *cef = fopen("compile_errors", "w");
	if (cef == NULL) {
		eprintf("Failed to open 'compile_errors' - %s\n", strerror(errno));
		return 1;
	}

	if (VERBOSE)
		printf("Compiling: '%s' ", (in).c_str());

	// Change stderr
	int stderr_backup = dup(STDERR_FILENO);
	dup2(fileno(cef), STDERR_FILENO);

	// Run compiler
	vector<string> args;
	append(args)("g++")("-O2")("-static")("-lm")(in)("-o")(out);

	/* Compile as 32 bit executable (not essential but if checker will be x86_63
	*  and Conver i386 then checker won't work, with it its more secure (see
	*  making i386 syscall from x86_64))
	*/
	append(args)("-m32");

	if (VERBOSE) {
		printf("(Command: '%s", args[0].c_str());
		for (size_t i = 1, end = args.size(); i < end; ++i)
			printf(" %s", args[i].c_str());
		printf("')\n");
	}

	int compile_status = spawn(args[0], args);

	// Change back stderr
	dup2(stderr_backup, STDERR_FILENO);
	close(stderr_backup);

	// Check for errors
	if (compile_status != 0) {
		eprintf("Compile errors:\n%s\n", getFileContents("compile_errors")
			.c_str());
		return 2;

	} else if (VERBOSE)
		printf("Completed successfully.\n");

	fclose(cef);
	return 0;
}

static void assignPoints() {
	int points = 100, groups_left = conf_cfg.test_groups.size();

	foreach (group, conf_cfg.test_groups) {
		assert(!group->tests.empty());

		pair<string, string> tag =
			TestNameCompatator::extractTag(group->tests[0].name);
		if (tag.first == "0" || tag.second == "ocen") {
			group->points = 0;
			continue;
		}

		group->points = points / groups_left--;
		points -= group->points;
	}
}

int setLimits(const string& package) {
	// Compile checker
	if (compile(package + "check/" + conf_cfg.checker, "checker") != 0)
			return 1;

	if (TIME_LIMIT > 0 && !GEN_OUT && !VALIDATE_OUT) {
		// Set time limits on tests
		foreach (group, conf_cfg.test_groups)
			foreach (test, group->tests) {
				test->time_limit = TIME_LIMIT;
				printf("  %-11s --- / %-4s\n", test->name.c_str(),
					usecToSec(test->time_limit, 2, false).c_str());
			}

		assignPoints();
		return 0;
	}

	// Compile solution
	if (conf_cfg.solution.empty()) {
		eprintf("Error: Solution not found\n");
		return 2;
	}

	if (compile(package + "prog/" + conf_cfg.solution, "exec") != 0)
			return 2;

	// Set limits
	sandbox::options sb_opt = {
		TIME_LIMIT > 0 ? TIME_LIMIT : HARD_TIME_LIMIT,
		conf_cfg.memory_limit << 10,
		fopen("/dev/null", "r"),
		fopen("answer", "w"),
		fopen("/dev/null", "w")
	};

	sandbox::options check_sb_opt = {
		10000000ull, // 10s
		256 << 20, // 256 MB
		fopen("/dev/null", "r"),
		fopen("checker_out", "w"),
		sb_opt.new_stderr // fopen("/dev/null", "w")
	};

	if (sb_opt.new_stdin == NULL) {
		eprintf("Failed to open '/dev/null' - %s\n", strerror(errno));
		return 3;
	}

	if (sb_opt.new_stdout == NULL) {
		eprintf("Failed to open '%sanswer' - %s\n", tmp_dir->name(),
			strerror(errno));
		fclose(sb_opt.new_stdin);
		return 3;
	}

	if (sb_opt.new_stderr == NULL) {
		eprintf("Failed to open '/dev/null' - %s\n", strerror(errno));
		fclose(sb_opt.new_stdin);
		fclose(sb_opt.new_stdout);
		return 3;
	}

	if (check_sb_opt.new_stdin == NULL) {
		eprintf("Failed to open '/dev/null' - %s\n", strerror(errno));
		fclose(sb_opt.new_stdin);
		fclose(sb_opt.new_stdout);
		fclose(sb_opt.new_stderr);
		return 3;
	}

	if (check_sb_opt.new_stdout == NULL) {
		eprintf("Failed to open '%schecker_out' - %s\n", tmp_dir->name(),
			strerror(errno));
		fclose(sb_opt.new_stdin);
		fclose(sb_opt.new_stdout);
		fclose(sb_opt.new_stderr);
		fclose(check_sb_opt.new_stdin);
		return 3;
	}

	/*if (check_sb_opt.new_stderr == NULL) {
		eprintf("Failed to open '/dev/null' - %s\n", strerror(errno));
		fclose(sb_opt.new_stdin);
		fclose(sb_opt.new_stdout);
		fclose(sb_opt.new_stderr);
		fclose(check_sb_opt.new_stdin);
		fclose(check_sb_opt.new_stdout);
		return 3;
	}*/

	vector<string> exec_args, checker_args(4);

	// Set time limits on tests
	foreach (group, conf_cfg.test_groups)
		foreach (test, group->tests) {
			if (USE_CONF && !FORCE_AUTO_LIMIT)
				sb_opt.time_limit = test->time_limit;
			// Reopening sb_opt.new_stdin
			if (freopen((package + "tests/" + test->name + ".in").c_str(), "r",
					sb_opt.new_stdin) == NULL) {
				eprintf("Failed to open: '%s' - %s\n",
					(package + "tests/" + test->name + ".in").c_str(),
					strerror(errno));
				fclose(sb_opt.new_stdout);
				fclose(sb_opt.new_stderr);
				return 4;
			}
			// Reopening sb_opt.new_stdout
			if (GEN_OUT && freopen((package + "tests/" + test->name + ".out")
					.c_str(), "w", sb_opt.new_stdout) == NULL) {
				eprintf("Failed to open: '%s' - %s\n",
					(package + "tests/" + test->name + ".out").c_str(),
					strerror(errno));
				fclose(sb_opt.new_stdin);
				fclose(sb_opt.new_stderr);
				return 4;
			}

			// Truncate sb_opt.new_stdout
			if (!GEN_OUT) {
				ftruncate(fileno(sb_opt.new_stdout), 0);
				rewind(sb_opt.new_stdout);
			}

			if (!QUIET) {
				printf("  %-11s ", test->name.c_str());
				fflush(stdout);
			}

			// Run
			sandbox::ExitStat es = sandbox::run("./exec", exec_args, &sb_opt);

			// Set time_limit (minimum time_limit is 0.1s unless TIME_LIMIT > 0)
			if (TIME_LIMIT > 0 || (FORCE_AUTO_LIMIT || !USE_CONF))
				test->time_limit = TIME_LIMIT > 0 ? TIME_LIMIT :
					std::max(es.runtime * 4, 100000ull);

			if (!QUIET) {
				printf("%4s / %-4s    Status: ", usecToSec(es.runtime, 2, false)
					.c_str(), usecToSec(test->time_limit, 2, false).c_str());

				if (es.code == 0)
					printf("\e[1;32mOK\e[m");
				else if (es.runtime < test->time_limit)
					printf("\e[1;33mRTE\e[m (%s)", es.message.c_str());
				else
					printf("\e[1;33mTLE\e[m");

				if (VERBOSE)
					printf("   Exited with %i [ %s ]", es.code,
						usecToSec(es.runtime, 6, false).c_str());
			}

			// Validate output
			if (VALIDATE_OUT) {
				checker_args[1] = package + "tests/" + test->name + ".in";
				checker_args[2] = package + "tests/" + test->name + ".out";
				checker_args[3] = GEN_OUT ? checker_args[2] : "answer";

				if (!QUIET) {
					printf("  Output validation... ");
					fflush(stdout);
				}

				// Truncate check_sb_opt.new_stdout
				ftruncate(fileno(check_sb_opt.new_stdout), 0);
				rewind(check_sb_opt.new_stdout);

				es = sandbox::run("./checker", checker_args, &check_sb_opt,
					sandbox::CheckerCallback(
						vector<string>(checker_args.begin() + 1,
							checker_args.end())));

				if (!QUIET) {
					if (es.code == 0)
						printf("\e[1;32mPASSED\e[m");

					else if (WIFEXITED(es.code) && WEXITSTATUS(es.code) == 1) {
						printf("\e[1;31mFAILED\e[m");

						FILE *f = fopen("checker_out", "r");
						if (f != NULL) {
							char buff[204] = {};
							ssize_t ret = fread(buff, 1, 204, f);

							// Remove trailing white characters
							while (ret > 0 && isspace(buff[ret - 1]))
								buff[--ret] = '\0';

							if (ret > 200)
								strncpy(buff + 200, "...", 4);

							printf(" \"%s\"", buff);
							fclose(f);
						}

					} else if (es.runtime < test->time_limit)
						printf("\e[1;33mRTE\e[m (%s)", es.message.c_str());

					else
						printf("\e[1;33mTLE\e[m");

					if (VERBOSE)
						printf("   Exited with %i [ %s ]", es.code,
							usecToSec(es.runtime, 6, false).c_str());
				}
			}

			if (!QUIET)
				printf("\n");
		}

	fclose(sb_opt.new_stdin);
	fclose(sb_opt.new_stdout);
	fclose(sb_opt.new_stderr);
	fclose(check_sb_opt.new_stdin);
	fclose(check_sb_opt.new_stdout);
	// fclose(check_sb_opt.new_stderr);

	if (!USE_CONF)
		assignPoints();
	return 0;
}
