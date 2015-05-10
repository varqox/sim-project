#pragma once

#include <string>
#include <vector>

struct spawn_opts {
	FILE *new_stdin; // NULL - left value unchanged
	FILE *new_stdout; // NULL - left value unchanged
	FILE *new_stderr; // NULL - left value unchanged
};

extern const spawn_opts default_spawn_opts; // { NULL, NULL, NULL }

/**
 * @brief Runs exec using execvp(3) with arguments @p args
 *
 * @param exec file to execute
 * @param args arguments
 * @param sopt spawn options - defines to what change stdin, stdout and stderr
 * NULL field means to left value unchanged form parent
 *
 * @return exit code on success, -1 on error
 */
int spawn(const std::string& exec, const std::vector<std::string>& args,
	const struct spawn_opts *opts = &default_spawn_opts);

/**
 * @brief Runs exec using execvp(3) with arguments @p args
 *
 * @param exec file to execute
 * @param argc number of arguments
 * @param args arguments
 * @param sopt spawn options - defines to what change stdin, stdout and stderr
 * NULL field means to left value unchanged form parent
 *
 * @return exit code on success, -1 on error
 */
int spawn(const std::string& exec, size_t argc, std::string *args,
	const struct spawn_opts *opts = &default_spawn_opts);
