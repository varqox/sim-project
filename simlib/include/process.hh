#pragma once

#include "inplace_buff.hh"

#include <algorithm>
#include <chrono>
#include <functional>
#include <linux/limits.h>
#include <optional>
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
InplaceBuff<PATH_MAX> get_cwd();

/**
 * @brief Get absolute path of executable of the process with pid @p pid
 * @details executable path is always absolute, notice that if executable is
 *   unlined then path will have additional " (deleted)" suffix
 *
 * @param pid - PID of a process of which the executable path will be obtained
 *
 * @return absolute path of @p pid executable
 *
 * @errors If readlink(2) fails then std::runtime_error will be thrown
 */
std::string executable_path(pid_t pid);

/**
 * @brief Get a vector of processes pids which are instances of one of
 *   @p exec_set
 * @details Function check every accessible process if matches
 *
 * @param exec_set paths to executables (if all are absolute, then getting CWD
 *   is omitted)
 * @param include_me whether include the calling process in the result
 *   if matches or not
 *
 * @return vector of pids of matched processes
 *
 * @errors Exceptions from get_cwd() or if opendir(2) fails then
 *   std::runtime_error will be thrown
 */
std::vector<pid_t>
find_processes_by_executable_path(std::vector<std::string> exec_set,
                                  bool include_me = false);
/**
 * @brief Kills processes that have executable files in @p exec_set
 * @details First tries with SIGTERM, but after @p wait_timeout sends SIGKILL if
 *   @p kill_after_waiting is true
 *
 * @param exec_set paths to executables (if absolute, getting CWD is omitted)
 * @param wait_timeout how long to wait for processes to die (if unset, wait
 *   indefinitely)
 * @param kill_after_waiting whether to send SIGKILL if process is still alive
 *   after wait_timeout
 */
void kill_processes_by_exec(
   std::vector<std::string> exec_set,
   std::optional<std::chrono::duration<double>> wait_timeout = std::nullopt,
   bool kill_after_waiting = false, int terminate_signal = SIGTERM);

/**
 * @brief Change current working directory to process's executable path
 * directory
 * @details Uses executable_path() and chdir(2)
 *
 * @errors Exceptions from executable_path() or if chdir(2) fails then
 *   std::runtime_error will be thrown
 */
void chdir_to_executable_dirpath();

constexpr int8_t ARCH_i386 = 0;
constexpr int8_t ARCH_x86_64 = 1;
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
int8_t detect_architecture(pid_t pid);

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
auto block_signals(F&& f, Args&&... args)
   -> decltype(f(std::forward<Args>(args)...)) {
	SignalBlocker sb;
	return f(std::forward<Args>(args)...);
}

#define BLOCK_SIGNALS(...) block_signals([&] { return __VA_ARGS__; })

// Block all signals when function @p f is called
template <class F, class... Args>
auto thread_block_signals(F&& f, Args&&... args)
   -> decltype(f(std::forward<Args>(args)...)) {
	ThreadSignalBlocker sb;
	return f(std::forward<Args>(args)...);
}

#define THREAD_BLOCK_SIGNALS(...)                                              \
	thread_block_signals([&] { return __VA_ARGS__; })
