#pragma once

#include "spawner.h"

#include <seccomp.h>

enum class OpenAccess {
	NONE, // Yes, it is possible if the file is opened with O_WRONLY | O_RDWR
	RDONLY,
	WRONLY,
	RDWR
};

class SyscallCallback {
public:
	// Resets the state of the Callback to state just after creation
	virtual void reset() noexcept {}

	// Returns whether to kill the tracee
	virtual bool operator()() = 0;

	virtual ~SyscallCallback() = default;
};

class Sandbox : protected Spawner {
public:
	using AllowedFile = std::pair<std::string, OpenAccess>;

private:
	pid_t tracee_pid_;
	std::vector<std::unique_ptr<SyscallCallback>> callbacks_;
	const std::vector<AllowedFile>* allowed_files_;

	scmp_filter_ctx x86_ctx_;
	scmp_filter_ctx x86_64_ctx_;

	std::string message_to_set_in_exit_stat_; // if non-empty it will be set in ExitStat

	FileDescriptor tracee_statm_fd_; // For tracking vm_peak (vm stands for virtual memory)
	uint64_t tracee_vm_peak_; // In pages

	/// Adds rule to x86_ctx_ and x86_64_ctx_
	template<class... T>
	void seccomp_rule_add_both_ctx(T&&... args);

	/// Constructs the message to be set in ExitStat from @p args and returns true
	template<class... T>
	bool set_message_callback(T&&... args);

	void reset_callbacks() noexcept;

	uint64_t get_tracee_vm_size();

public:
	Sandbox();

	~Sandbox() {
		seccomp_release(x86_64_ctx_);
		seccomp_release(x86_ctx_);
	}

	using Spawner::ExitStat;
	using Spawner::Options;

	/**
	 * @brief Runs @p exec with arguments @p exec_args and limits:
	 *   @p opts.time_limit and @p opts.memory_limit under seccomp(2) and
	 *   ptrace(2)
	 * @details
	 *   @p exec is called via execvp()
	 *   This function is thread-safe.
	 *   IMPORTANT: To function properly this function uses internally signals
	 *     SIGRTMIN and SIGRTMIN + 1 and installs handlers for them. So be aware
	 *     that using these signals while this function runs (in any thread) is
	 *     not safe. Moreover if your program installed handler for the above
	 *     signals, it must install them again after the function returns.
	 *
	 * @param exec path to file will be executed
	 * @param exec_args arguments passed to exec
	 * @param opts options (new_stdin_fd, new_stdout_fd, new_stderr_fd - file
	 *   descriptors to which respectively stdin, stdout, stderr of sandboxed
	 *   process will be changed or if negative, closed;
	 *   time_limit set to 0 disables the time limit;
	 *   memory_limit set to 0 disables memory limit;
	 *   working_dir set to "", "." or "./" disables changing working directory)
	 * @param func callback functor (used to determine if a syscall should be
	 *   executed)
	 *
	 * @return Returns ExitStat structure with fields:
	 *   - runtime: in timespec structure {sec, nsec}
	 *   - si: {
	 *       code: si_code form siginfo_t from waitid(2)
	 *       status: si_status form siginfo_t from waitid(2)
	 *     }
	 *   - rusage: resource used (see getrusage(2)).
	 *   - vm_peak: peak virtual memory size [bytes]
	 *   - message: detailed info about error, etc.
	 *
	 * @errors Throws an exception std::runtime_error with appropriate
	 *   information if any syscall fails
	 */
	ExitStat run(CStringView exec, const std::vector<std::string>& exec_args,
		const Options& opts = Options(),
		const std::vector<AllowedFile>& allowed_files = {});
};
