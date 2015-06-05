#include "convert_package.h"
#include "set_limits.h"

#include "../include/compile.h"
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

static void assignPoints() {
	int points = 100, groups_left = conf_cfg.test_groups.size();

	foreach (group, conf_cfg.test_groups) {
		assert(!group->tests.empty());

		pair<string, string> tag =
			TestNameCompatator::extractTag(group->tests[0].name);
		if (tag.first == "0" || tag.second == "ocen") {
			group->points = 0;
			--groups_left;
			continue;
		}

		group->points = points / groups_left--;
		points -= group->points;
	}
}

int setLimits(const string& package) {
	// Compile checker
	if (VERBOSE)
		printf("Compiling checker...\n");

	if (compile(package + "check/" + conf_cfg.checker, "checker") != 0) {
		if (VERBOSE)
			printf("Failed.\n");

		return 1;
	}

	if (VERBOSE)
		printf("Completed successfully.\n");

	if (TIME_LIMIT > 0 && !GEN_OUT && !VALIDATE_OUT) {
		// Set time limits on tests
		foreach (group, conf_cfg.test_groups)
			foreach (test, group->tests) {
				test->time_limit = TIME_LIMIT;
				printf("  %-11s --- / %-4s\n", test->name.c_str(),
					usecToSecStr(test->time_limit, 2, false).c_str());
			}

		assignPoints();
		return 0;
	}

	// Compile solution
	if (conf_cfg.solution.empty()) {
		eprintf("Error: Solution not found\n");
		return 2;
	}

	if (VERBOSE)
		printf("Compiling solution...\n");

	if (compile(package + "prog/" + conf_cfg.solution, "exec") != 0) {
		if (VERBOSE)
			printf("Failed.\n");

		return 2;
	}

	if (VERBOSE)
		printf("Completed successfully.\n");

	// Set limits
	// Prepare runtime environment
	sandbox::options sb_opts = {
		TIME_LIMIT > 0 ? TIME_LIMIT : HARD_TIME_LIMIT,
		conf_cfg.memory_limit << 10,
		-1,
		open("answer", O_WRONLY | O_CREAT | O_TRUNC),
		-1
	};

	// Check for errors
	if (sb_opts.new_stdout_fd < 0) {
		eprintf("Failed to open '%sanswer' - %s\n", tmp_dir->name(),
			strerror(errno));
		return 3;
	}

	sandbox::options checker_sb_opts = {
		10 * 1000000, // 10s
		256 << 20, // 256 MB
		-1,
		open("checker_out", O_WRONLY | O_CREAT | O_TRUNC),
		-1
	};

	// Check for errors
	if (checker_sb_opts.new_stdout_fd < 0) {
		eprintf("Failed to open '%schecker_out' - %s\n", tmp_dir->name(),
			strerror(errno));

		// Close file descriptors
		while (close(sb_opts.new_stdout_fd) == -1 && errno == EINTR) {}
		return 3;
	}

	vector<string> exec_args, checker_args(4);

	// Set time limits on tests
	foreach (group, conf_cfg.test_groups)
		foreach (test, group->tests) {
			if (USE_CONF && !FORCE_AUTO_LIMIT)
				sb_opts.time_limit = test->time_limit;

			// Reopen file descriptors
			// Close sb_opts.new_stdin_fd
			if (sb_opts.new_stdin_fd >= 0)
				while (close(sb_opts.new_stdin_fd) == -1 && errno == EINTR) {}

			// If !GEN_OUT, close sb_opts.new_stdout_fd
			if (GEN_OUT)
				while (close(sb_opts.new_stdout_fd) == -1 && errno == EINTR) {}

			// Open sb_opts.new_stdin_fd
			if ((sb_opts.new_stdin_fd = open(
					(package + "tests/" + test->name + ".in").c_str(),
					O_RDONLY | O_LARGEFILE | O_NOFOLLOW)) == -1) {
				eprintf("Failed to open: '%s' - %s\n",
					(package + "tests/" + test->name + ".in").c_str(),
					strerror(errno));

				// Close file descriptors
				while (close(checker_sb_opts.new_stdout_fd) == -1 &&
					errno == EINTR) {}
				return 4;
			}

			// Open sb_opts.new_stdout_fd
			if (GEN_OUT && (sb_opts.new_stdout_fd = open(
					(package + "tests/" + test->name + ".out").c_str(),
					O_WRONLY | O_CREAT | O_TRUNC)) == -1) {
				eprintf("Failed to open: '%s' - %s\n",
					(package + "tests/" + test->name + ".out").c_str(),
					strerror(errno));

				// Close file descriptors
				while (close(checker_sb_opts.new_stdout_fd) == -1 &&
					errno == EINTR) {}
				return 4;
			}

			// If !GEN_OUT, truncate sb_opts.new_stdout_fd ("answer" file)
			if (!GEN_OUT) {
				ftruncate(sb_opts.new_stdout_fd, 0);
				lseek(sb_opts.new_stdout_fd, 0, SEEK_SET);
			}

			if (!QUIET) {
				printf("  %-11s ", test->name.c_str());
				fflush(stdout);
			}

			// Run
			sandbox::ExitStat es = sandbox::run("./exec", exec_args, &sb_opts);

			// Set time_limit (minimum time_limit is 0.1s unless TIME_LIMIT > 0)
			if (TIME_LIMIT > 0 || (FORCE_AUTO_LIMIT || !USE_CONF))
				test->time_limit = TIME_LIMIT > 0 ? TIME_LIMIT :
					std::max(es.runtime * 4, 100000ull);

			if (!QUIET) {
				printf("%4s / %-4s    Status: ",
					usecToSecStr(es.runtime, 2, false).c_str(),
					usecToSecStr(test->time_limit, 2, false).c_str());

				if (es.code == 0)
					printf("\e[1;32mOK\e[m");
				else if (es.runtime < test->time_limit)
					printf("\e[1;33mRTE\e[m (%s)", es.message.c_str());
				else
					printf("\e[1;33mTLE\e[m");

				if (VERBOSE)
					printf("   Exited with %i [ %s ]", es.code,
						usecToSecStr(es.runtime, 6, false).c_str());
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

				// Truncate checker_sb_opts.new_stdout_fd
				ftruncate(checker_sb_opts.new_stdout_fd, 0);
				lseek(checker_sb_opts.new_stdout_fd, 0, SEEK_SET);

				es = sandbox::run("./checker", checker_args, &checker_sb_opts,
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
							usecToSecStr(es.runtime, 6, false).c_str());
				}
			}

			if (!QUIET)
				printf("\n");
		}

	// Close file descriptors
	if (sb_opts.new_stdin_fd)
		while (close(sb_opts.new_stdin_fd) == -1 && errno == EINTR) {}
	while (close(sb_opts.new_stdout_fd) == -1 && errno == EINTR) {}
	while (close(checker_sb_opts.new_stdout_fd) == -1 && errno == EINTR) {}

	if (!USE_CONF)
		assignPoints();
	return 0;
}
