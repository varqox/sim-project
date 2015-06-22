#include "../include/debug.h"
#include "../include/process.h"

#include <cerrno>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

using std::string;
using std::vector;

const spawn_opts default_spawn_opts = {
	STDIN_FILENO,
	STDOUT_FILENO,
	STDERR_FILENO
};

int spawn(const char* exec, const char* args[], const struct spawn_opts *opts) {
	pid_t cpid = fork();
	if (cpid == -1) {
		eprintf("%s:%i: %s: Failed to fork() - %s\n", __FILE__, __LINE__,
			__PRETTY_FUNCTION__, strerror(errno));
		return -1;

	} else if (cpid == 0) {
		// Change stdin
		if (opts->new_stdin_fd < 0)
			while (close(STDIN_FILENO) == -1 && errno == EINTR) {}

		else if (opts->new_stdin_fd != STDIN_FILENO)
			while (dup2(opts->new_stdin_fd, STDIN_FILENO) == -1)
				if (errno != EINTR)
					_exit(-1);

		// Change stdout
		if (opts->new_stdout_fd < 0)
			while (close(STDOUT_FILENO) == -1 && errno == EINTR) {}

		else if (opts->new_stdout_fd != STDOUT_FILENO)
			while (dup2(opts->new_stdout_fd, STDOUT_FILENO) == -1)
				if (errno != EINTR)
					_exit(-1);

		// Change stderr
		if (opts->new_stderr_fd < 0)
			while (close(STDERR_FILENO) == -1 && errno == EINTR) {}

		else if (opts->new_stderr_fd != STDERR_FILENO)
			while (dup2(opts->new_stderr_fd, STDERR_FILENO) == -1)
				if (errno != EINTR)
					_exit(-1);

		execvp(exec, (char *const *)args);
		_exit(-1);
	}

	int status;
	waitpid(cpid, &status, 0);
	return status;
};

int spawn(const string& exec, const vector<string>& args,
		const struct spawn_opts *opts) {
	// Convert args
	const size_t len = args.size();
	const char *argv[len + 1];
	argv[len] = NULL;

	for (size_t i = 0; i < len; ++i)
		argv[i] = args[i].c_str();

	return spawn(exec.c_str(), argv, opts);
}

int spawn(const string& exec, size_t argc, string *args,
		const struct spawn_opts *opts) {
	// Convert args
	const size_t len = argc;
	const char *argv[len + 1];
	argv[len] = NULL;

	for (size_t i = 0; i < len; ++i)
		argv[i] = args[i].c_str();

	return spawn(exec.c_str(), argv, opts);
}