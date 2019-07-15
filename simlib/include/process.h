#pragma once

#include "string.h"

#include <algorithm>
#include <linux/limits.h>
#include <string>
#include <sys/wait.h>
#include <vector>

/**
 * @brief Get current working directory
 * @details Uses get_current_dir_name()
 *
 * @return current working directory (absolute path, with trailing '/')
 *
 * @errors If get_current_dir_name() fails then std::runtime_error will be
 *   thrown
 */
InplaceBuff<PATH_MAX> getCWD();

/**
 * @brief Get a process with pid @p pid executable path
 * @details executable path is always absolute, notice that if executable is
 *   removed then path will have additional " (deleted)" suffix
 *
 * @param pid - PID of a process of which the executable path will be obtained
 *
 * @return absolute path of @p pid executable
 *
 * @errors If readlink(2) fails then std::runtime_error will be thrown
 */
std::string getExec(pid_t pid);

/**
 * @brief Get a vector of processes pids which are instances of one of
 *   @p exec_set
 * @details Function check every accessible process if matches
 *
 * @param exec_set paths to executables (if absolute, then getting CWD is
 *   omitted)
 * @param include_me whether include the calling process in the result
 *   (if matches) or not
 *
 * @return vector of pids of matched processes
 *
 * @errors Exceptions from getCWD() or if opendir(2) fails then
 *   std::runtime_error will be thrown
 */
std::vector<pid_t> findProcessesByExec(std::vector<std::string> exec_set,
                                       bool include_me = false);

/**
 * @brief Returns the process executable directory
 * @details Uses getExec()
 *
 * @param pid - PID of a process of which the executable directory will be
 *   obtained
 *
 * @return The current process executable directory - absolute path with
 *   trailing '/'
 *
 * @errors Exceptions from getExec()
 */
std::string getExecDir(pid_t pid);

/**
 * @brief Change current working directory to process executable directory
 * @details Uses getExecDir() and chdir(2)
 *
 * @return New CWD (with trailing '/')
 *
 * @errors Exceptions from getExecDir() or if chdir(2) fails then
 *   std::runtime_error will be thrown
 */
std::string chdirToExecDir();

inline constexpr int8_t ARCH_i386 = 0;
inline constexpr int8_t ARCH_x86_64 = 1;
/**
 * @brief Detects architecture of running process @p pid
 * @details Currently it only detects i386 and x86_64 (see defined ARCH_ values
 *   above)
 *
 * @param pid pid of process to detect architecture
 *
 * @return detected architecture
 *
 * @errors If architecture is different from allowed or any error occurs
 *   an exception of type std::runtime_error is thrown
 */
int8_t detectArchitecture(pid_t pid);

/**
 * @brief Returns field @p field_no value from /proc/@p pid/stat
 *
 * @param pid process id
 * @param field_no number of the field to return, field numeration goes from 0
 *
 * @return field value
 *
 * @errors If any error occurs then an exception of type std::runtime_error is
 *   thrown with a proper message. Example errors: process not found,
 *   invalid field_no...
 */
std::string getProcStat(pid_t pid, uint field_no);

// Block all signals
template <int (*func)(int, const sigset_t*, sigset_t*)>
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

template <int (*func)(int, const sigset_t*, sigset_t*)>
const sigset_t SignalBlockerBase<func>::empty_mask =
   SignalBlockerBase<func>::empty_mask_val();

template <int (*func)(int, const sigset_t*, sigset_t*)>
const sigset_t SignalBlockerBase<func>::full_mask =
   SignalBlockerBase<func>::full_mask_val();

typedef SignalBlockerBase<sigprocmask> SignalBlocker;
typedef SignalBlockerBase<pthread_sigmask> ThreadSignalBlocker;

// Block all signals when function @p f is called
template <class F, class... Args>
auto blockSignals(F&& f, Args&&... args)
   -> decltype(f(std::forward<Args>(args)...)) {
	SignalBlocker sb;
	return f(std::forward<Args>(args)...);
}

#define BLOCK_SIGNALS(...) blockSignals([&] { return __VA_ARGS__; })

// Block all signals when function @p f is called
template <class F, class... Args>
auto threadBlockSignals(F&& f, Args&&... args)
   -> decltype(f(std::forward<Args>(args)...)) {
	ThreadSignalBlocker sb;
	return f(std::forward<Args>(args)...);
}

#define THREAD_BLOCK_SIGNALS(...)                                              \
	threadBlockSignals([&] { return __VA_ARGS__; })
