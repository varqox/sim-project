#pragma once

#include <string>
#include <vector>

struct spawn_opts {
	int new_stdin_fd; // negative - close, STDIN_FILENO - do not change
	int new_stdout_fd; // negative - close, STDOUT_FILENO - do not change
	int new_stderr_fd; // negative - close, STDERR_FILENO - do not change
};

extern const spawn_opts default_spawn_opts; /* = {
	STDIN_FILENO,
	STDOUT_FILENO,
	STDERR_FILENO
}; */

/**
 * @brief Runs exec using execvp(3) with arguments @p args
 *
 * @param exec file to execute
 * @param args arguments (last has to be NULL)
 * @param sopt spawn options - defines to what change stdin, stdout and stderr
 * negative field means to close stream
 *
 * @return exit code on success, -1 on error
 */
int spawn(const char* exec, const char* args[],
	const struct spawn_opts *opts = &default_spawn_opts);

/**
 * @brief Runs exec using execvp(3) with arguments @p args
 *
 * @param exec file to execute
 * @param args arguments (last has to be NULL)
 * @param sopt spawn options - defines to what change stdin, stdout and stderr
 * negative field means to close stream
 *
 * @return exit code on success, -1 on error
 */
inline int spawn(const std::string& exec, const char* args[],
		const struct spawn_opts *opts = &default_spawn_opts) {
	return spawn(exec.c_str(), args, opts);
}

/**
 * @brief Runs exec using execvp(3) with arguments @p args
 *
 * @param exec file to execute
 * @param args arguments
 * @param sopt spawn options - defines to what change stdin, stdout and stderr
 * negative field means to close stream
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
 * negative field means to close stream
 *
 * @return exit code on success, -1 on error
 */
int spawn(const std::string& exec, size_t argc, std::string *args,
	const struct spawn_opts *opts = &default_spawn_opts);
