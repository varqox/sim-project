#include "convert_package.h"

#include "../simlib/include/compile.h"
#include "../simlib/include/debug.h"
#include "../simlib/include/sandbox.h"
#include "../simlib/include/sandbox_checker_callback.h"

#include <cassert>

using std::pair;
using std::string;
using std::vector;

static void assignPoints() {
	int points = 100, groups_left = config_conf.test_groups.size();

	for (auto& group : config_conf.test_groups) {
		assert(!group.tests.empty());

		pair<string, string> tag =
			TestNameCompatator::extractTag(group.tests[0].name);
		if (tag.first == "0" || tag.second == "ocen") {
			group.points = 0;
			--groups_left;
			continue;
		}

		group.points = points / groups_left--;
		points -= group.points;
	}
}

int setLimits(const string& package_path) {
	try {
		// Compile checker
		if (compile(concat(package_path, "check/", config_conf.checker),
				"checker", (VERBOSITY >> 1) + 1, nullptr, 0, PROOT_PATH) != 0) {
			if (VERBOSITY == 1)
				eprintf("Checker compilation failed.\n");
			return 1;
		}

		if (TIME_LIMIT > 0 && !GEN_OUT && !VALIDATE_OUT) {
			// Set time limits on tests
			for (auto& group : config_conf.test_groups)
				for (auto& test : group.tests) {
					test.time_limit = TIME_LIMIT;
					printf("  %-11s --- / %-4s\n", test.name.c_str(),
						usecToSecStr(test.time_limit, 2, false).c_str());
				}

			assignPoints();
			return 0;
		}

		// Compile solution
		if (config_conf.main_solution.empty()) {
			eprintf("Error: main solution not found\n");
			return 2;
		}

		if (compile(concat(package_path, "prog/", config_conf.main_solution),
				"exec", (VERBOSITY >> 1) + 1, nullptr, 0, PROOT_PATH) != 0) {
			if (VERBOSITY == 1)
				eprintf("Solution compilation failed.\n");
			return 2;
		}

	} catch (const std::exception& e) {
		eprintf("Compilation failed %s\n", e.what());
		return 1;
	}

	// Set limits
	// Prepare runtime environment
	sandbox::options sb_opts = {
		TIME_LIMIT > 0 ? TIME_LIMIT : HARD_TIME_LIMIT,
		config_conf.memory_limit << 10,
		-1,
		open("answer", O_WRONLY | O_CREAT | O_TRUNC,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), // (mode: 0644/rw-r--r--)
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
		open("checker_out", O_WRONLY | O_CREAT | O_TRUNC,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), // (mode: 0644/rw-r--r--)
		-1
	};

	// Check for errors
	if (checker_sb_opts.new_stdout_fd < 0) {
		eprintf("Failed to open '%schecker_out' - %s\n", tmp_dir->name(),
			strerror(errno));

		// Close file descriptors
		sclose(sb_opts.new_stdout_fd);
		return 3;
	}

	vector<string> exec_args, checker_args(4);

	// Set time limits on tests
	for (auto& group : config_conf.test_groups)
		for (auto& test : group.tests) {
			if (USE_CONFIG && !FORCE_AUTO_LIMIT)
				sb_opts.time_limit = test.time_limit;

			// Reopen file descriptors
			// Close sb_opts.new_stdin_fd
			if (sb_opts.new_stdin_fd >= 0)
				sclose(sb_opts.new_stdin_fd);

			// If !GEN_OUT, close sb_opts.new_stdout_fd
			if (GEN_OUT)
				sclose(sb_opts.new_stdout_fd);

			// Open sb_opts.new_stdin_fd
			if ((sb_opts.new_stdin_fd = open(
					concat(package_path, "tests/", test.name, ".in").c_str(),
					O_RDONLY | O_LARGEFILE | O_NOFOLLOW)) == -1) {
				eprintf("Failed to open: '%s' - %s\n",
					concat(package_path, "tests/", test.name, ".in").c_str(),
					strerror(errno));

				// Close file descriptors
				sclose(checker_sb_opts.new_stdout_fd);
				return 4;
			}

			// Open sb_opts.new_stdout_fd
			if (GEN_OUT && (sb_opts.new_stdout_fd = open(
					concat(package_path, "tests/", test.name, ".out").c_str(),
					O_WRONLY | O_CREAT | O_TRUNC, // (mode: 0644/rw-r--r--)
					S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
				eprintf("Failed to open: '%s' - %s\n",
					concat(package_path, "tests/", test.name, ".out").c_str(),
					strerror(errno));

				// Close file descriptors
				sclose(checker_sb_opts.new_stdout_fd);
				return 4;
			}

			// If !GEN_OUT, truncate sb_opts.new_stdout_fd ("answer" file)
			if (!GEN_OUT) {
				ftruncate(sb_opts.new_stdout_fd, 0);
				lseek(sb_opts.new_stdout_fd, 0, SEEK_SET);
			}

			if (VERBOSITY > 0) {
				printf("  %-11s ", test.name.c_str());
				fflush(stdout);
			}

			// Run
			sandbox::ExitStat es = sandbox::run("./exec", exec_args, &sb_opts);

			// Set time_limit (adjust time_limit by 0.4s unless TIME_LIMIT > 0)
			if (TIME_LIMIT > 0 || (FORCE_AUTO_LIMIT || !USE_CONFIG))
				test.time_limit = TIME_LIMIT > 0 ? TIME_LIMIT :
					es.runtime * 4 + 400000ull;

			if (VERBOSITY > 0) {
				printf("%4s / %-4s    Status: ",
					usecToSecStr(es.runtime, 2, false).c_str(),
					usecToSecStr(test.time_limit, 2, false).c_str());

				if (es.code == 0)
					printf("\e[1;32mOK\e[m");
				else if (es.runtime < test.time_limit)
					printf("\e[1;33mRTE\e[m (%s)", es.message.c_str());
				else
					printf("\e[1;33mTLE\e[m");

				if (VERBOSITY > 1)
					printf("   Exited with %i [ %s ]", es.code,
						usecToSecStr(es.runtime, 6, false).c_str());
			}

			// Validate output
			if (VALIDATE_OUT) {
				checker_args[1] = concat(package_path, "tests/", test.name, ".in");
				checker_args[2] = concat(package_path, "tests/", test.name, ".out");
				checker_args[3] = (GEN_OUT ? checker_args[2] : "answer");

				if (VERBOSITY > 0) {
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

				if (VERBOSITY > 0) {
					if (es.code == 0)
						printf("\e[1;32mPASSED\e[m");

					else if (WIFEXITED(es.code) && WEXITSTATUS(es.code) == 1) {
						printf("\e[1;31mFAILED\e[m");

						FILE *f = fopen("checker_out", "r");
						if (f != nullptr) {
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

					} else if (es.runtime < test.time_limit)
						printf("\e[1;33mRTE\e[m (%s)", es.message.c_str());

					else
						printf("\e[1;33mTLE\e[m");

					if (VERBOSITY > 1)
						printf("   Exited with %i [ %s ]", es.code,
							usecToSecStr(es.runtime, 6, false).c_str());
				}
			}

			if (VERBOSITY > 0)
				printf("\n");
		}

	// Close file descriptors
	if (sb_opts.new_stdin_fd)
		sclose(sb_opts.new_stdin_fd);
	sclose(sb_opts.new_stdout_fd);
	sclose(checker_sb_opts.new_stdout_fd);

	if (!USE_CONFIG)
		assignPoints();
	return 0;
}
