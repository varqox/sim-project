#pragma once

#include <csignal>
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
 * @param sopt spawn options - defines what to change stdin, stdout and
 *   stderr to (negative field closes the stream)
 * @param working_dir directory at which exec will be run, nullptr - do not
 *   change
 *
 * @return exit code on success, -1 on error
 */
int spawn(const char* exec, const char* args[],
	const struct spawn_opts *opts = &default_spawn_opts,
	const char* working_dir = nullptr);

/**
 * @brief Runs exec using execvp(3) with arguments @p args
 *
 * @param exec file to execute
 * @param args arguments (last has to be NULL)
 * @param sopt spawn options - defines what to change stdin, stdout and
 *   stderr to (negative field closes the stream)
 * @param working_dir directory at which exec will be run
 *
 * @return exit code on success, -1 on error
 */
inline int spawn(const std::string& exec, const char* args[],
		const struct spawn_opts *opts = &default_spawn_opts,
		const std::string& working_dir = ".") {
	return spawn(exec.c_str(), args, opts, working_dir);
}

/**
 * @brief Runs exec using execvp(3) with arguments @p args
 *
 * @param exec file to execute
 * @param args arguments
 * @param sopt spawn options - defines what to change stdin, stdout and
 *   stderr to (negative field closes the stream)
 * @param working_dir directory at which exec will be run
 *
 * @return exit code on success, -1 on error
 */
int spawn(const std::string& exec, const std::vector<std::string>& args,
	const struct spawn_opts *opts = &default_spawn_opts,
	const std::string& working_dir = ".");

/**
 * @brief Runs exec using execvp(3) with arguments @p args
 *
 * @param exec file to execute
 * @param argc number of arguments
 * @param args arguments
 * @param sopt spawn options - defines what to change stdin, stdout and
 *   stderr to (negative field closes the stream)
 * @param working_dir directory at which exec will be run
 *
 * @return exit code on success, -1 on error
 */
int spawn(const std::string& exec, size_t argc, std::string *args,
	const struct spawn_opts *opts = &default_spawn_opts,
	const std::string& working_dir = ".");

/**
 * @brief Get current working directory
 * @details Uses get_current_dir_name()
 *
 * @return current working directory (absolute path, with trailing '/')
 *
 * @errors If get_current_dir_name() fails then std::runtime_error will be
 *   thrown
 */
std::string getCWD() noexcept(false);

/**
 * @brief Get a process with pid @p pid executable path
 * @details executable path is always absolute, notice that if executable is
 *   removed then path will have additional " (deleted)" suffix
 *
 * @param pid process pid
 *
 * @return absolute path of @p pid executable
 *
 * @errors If readlink(2) fails then std::runtime_error will be thrown
 */
std::string getExec(pid_t pid) noexcept(false);

/**
 * @brief Get a vector of processes pids which are instances of @p exec
 * @details Function check every accessible process if matches
 *
 * @param exec path to executable (if absolute, then getting CWD is omitted)
 * @param include_me whether include calling process in result (if matches) or
 *   not
 *
 * @return vector of pids of matched processes
 *
 * @errors Exceptions from getCWD() or if opendir(2) fails then
 *   std::runtime_error will be thrown
 */
std::vector<pid_t> findProcessesByExec(std::string exec,
	bool include_me = false) noexcept(false);

/**
 * @brief Change current working directory to process executable directory
 * @details Uses getExec() and chdir(2)
 *
 * @return New CWD (with trailing '/')
 *
 * @errors Exceptions from getExec() or if chdir(2) fails then
 *   std::runtime_error will be thrown
 */
std::string chdirToExecDir() noexcept(false);

// Block all signals
template<int (*func)(int, const sigset_t*, sigset_t*)>
class SignalBlockerBase {
private:
	sigset_t old_mask;

public:
	const static sigset_t empty_mask, full_mask;

	SignalBlockerBase() noexcept { block(); }

	int block() noexcept { return func(SIG_SETMASK, &full_mask, &old_mask); }

	int unblock() noexcept { return func(SIG_SETMASK, &old_mask, nullptr); }

	~SignalBlockerBase() noexcept { unblock(); }

private:
	static sigset_t empty_mask_val() {
		sigset_t mask;
		sigemptyset(&mask);
		return mask;
	};

	static sigset_t full_mask_val() {
		sigset_t mask;
		sigfillset(&mask);
		return mask;
	};
};

template<int (*func)(int, const sigset_t*, sigset_t*)>
const sigset_t SignalBlockerBase<func>::empty_mask =
	SignalBlockerBase<func>::empty_mask_val();

template<int (*func)(int, const sigset_t*, sigset_t*)>
const sigset_t SignalBlockerBase<func>::full_mask =
	SignalBlockerBase<func>::full_mask_val();

typedef SignalBlockerBase<sigprocmask> SignalBlocker;
typedef SignalBlockerBase<pthread_sigmask> ThreadSignalBlocker;

// Block all signals when function @p f is called
template<class F, class... T>
auto blockSignals(F f, T... args) -> decltype(f(args...)) {
	SignalBlocker sb;
	return f(args...);
}

// Block all signals when function @p f is called
template<class F, class... T>
auto ThreadBlockSignals(F f, T... args) -> decltype(f(args...)) {
	ThreadSignalBlocker sb;
	return f(args...);
}
