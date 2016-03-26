#pragma once

#include "process.h"

#include <cstddef>
#include <sys/ptrace.h>

class Sandbox : protected Spawner {
public:
	Sandbox() = delete;

	struct i386_user_regs_struct;

	struct x86_64_user_regs_struct;

	class CallbackBase {
	protected:
		/**
		 * @brief Invokes ptr as function with parameters @p a, @p b
		 *
		 * @param pid traced process (via ptrace) pid
		 * @param arch architecture: 0 - i386, 1 - x86_64
		 * @param syscall currently invoked syscall
		 * @param allowed_filed files which can be opened
		 *
		 * @return true if call is allowed, false otherwise
		 */
		bool isSyscallAllowed(pid_t pid, int arch, int syscall,
			const std::vector<std::string>& allowed_files = {});

	public:
		/**
		 * @brief Checks whether syscall is allowed or not
		 *
		 * @param pid sandboxed process id
		 * @param syscall executed syscall number to check
		 *
		 * @return true - executed syscall is allowed, false - not allowed
		 */
		virtual bool operator()(pid_t pid, int syscall) = 0;

		virtual ~CallbackBase() noexcept {}
	};

	struct DefaultCallback : public CallbackBase {
		struct Pair {
			int syscall;
			int limit;
		};

		int functor_call; // number of operator() calls
		int8_t arch; // arch - architecture: 0 - i386, 1 - x86_64
		std::vector<Pair> limited_syscalls[2] = {
			{ /* i386 */
				{  11, 1 }, // SYS_execve
				{  33, 1 }, // SYS_access
				{  85, 1 }, // SYS_readlink
				{ 122, 1 }, // SYS_uname
				{ 243, 1 }, // SYS_set_thread_area
			},
			{ /* x86_64 */
				{  21, 1 }, // SYS_access
				{  59, 1 }, // SYS_execve
				{  63, 1 }, // SYS_uname
				{  89, 1 }, // SYS_readlink
				{ 158, 1 }, // SYS_arch_prctl
				{ 205, 1 }, // SYS_set_thread_area
			}
		};

		DefaultCallback() : functor_call(0), arch(-1) {}

		bool operator()(pid_t pid, int syscall);
	};

	using Spawner::ExitStat;
	using Spawner::Options;

	/**
	 * @brief Runs @p exec with arguments @p args with limits @p opts.time_limit
	 *   and @p opts.memory_limit under ptrace(2)
	 * @details Callback object is called on every syscall entry called by exec
	 *   with parameters: child pid, syscall number.
	 *   Callback::operator() must return whether syscall is allowed or not
	 *   @p exec is called via execvp()
	 *   This function is thread-safe
	 *
	 * @param exec filename that is to be executed
	 * @param args arguments passed to exec
	 * @param opts options (new_stdin_fd, new_stdout_fd, new_stderr_fd - file
	 *   descriptors to which respectively stdin, stdout, stderr of sandboxed
	 *   process will be changed or if negative, closed;
	 *   time_limit set to 0 disables time limit;
	 *   memory_limit set to 0 disables memory limit)
	 * @param working_dir directory at which @p exec will be run
	 * @param func callback functor (used to determine if syscall should be
	 *   executed)
	 *
	 * @return Returns ExitStat structure with fields:
	 *   - code: return status (in the format specified in wait(2)).
	 *   - runtime: in microseconds [usec]
	 *   - message: detailed info about error, disallowed syscall, etc.
	 *
	 * @errors Throws an exception std::runtime_error with appropriate
	 *   information if any syscall fails
	 */
	template<class Callback = DefaultCallback>
	static ExitStat run(const std::string& exec,
		const std::vector<std::string>& args, const Options& opts,
		const std::string& working_dir = ".",
		Callback&& func = Callback()) noexcept(false)
	{
		static_assert(std::is_base_of<CallbackBase, Callback>::value,
			"Callback has to derive from Sandbox::CallbackBase");
		return Spawner::runWithTimer<Impl, Callback>(opts.time_limit, exec,
			args, opts, working_dir, std::forward<Callback>(func));
	}

private:
	template<class Callback, class Timer>
	struct Impl {
		/**
		 * @brief Executes @p exec with arguments @p args with limits @p
		 *   opts.time_limit and @p opts.memory_limit under ptrace(2)
		 * @details Callback object is called on every syscall entry called by
		 *   exec with parameters: child pid, syscall number.
		 *   Callback::operator() must return whether syscall is allowed or not
		 *   @p exec is called via execvp()
		 *   This function is not thread-safe
		 *
		 * @param exec filename that is to be executed
		 * @param args arguments passed to exec
		 * @param opts options (new_stdin_fd, new_stdout_fd, new_stderr_fd -
		 *   - file descriptors to which respectively stdin, stdout, stderr of
		 *   sandboxed process will be changed or if negative, closed;
		 *   time_limit set to 0 disables time limit;
		 *   memory_limit set to 0 disables memory limit)
		 * @param working_dir directory at which @p exec will be run
		 * @param func callback functor (used to determine if syscall should be
		 *   executed)
		 *
		 * @return Returns ExitStat structure with fields:
		 *   - code: return status (in the format specified in wait(2)).
		 *   - runtime: in microseconds [usec]
		 *   - message: detailed info about error, disallowed syscall, etc.
		 *
		 * @errors Throws an exception std::runtime_error with appropriate
		 *   information if any syscall fails
		 */
		static ExitStat execute(const std::string& exec,
			const std::vector<std::string>& args, const Options& opts,
			const std::string& working_dir, Callback func) noexcept (false);
	};
};


/******************************* IMPLEMENTATION *******************************/

struct Sandbox::i386_user_regs_struct {
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint32_t ebp;
	uint32_t eax;
	uint32_t xds;
	uint32_t xes;
	uint32_t xfs;
	uint32_t xgs;
	uint32_t orig_eax;
	uint32_t eip;
	uint32_t xcs;
	uint32_t eflags;
	uint32_t esp;
	uint32_t xss;
};

struct Sandbox::x86_64_user_regs_struct {
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t rbp;
	uint64_t rbx;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rax;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t orig_rax;
	uint64_t rip;
	uint64_t cs;
	uint64_t eflags;
	uint64_t rsp;
	uint64_t ss;
	uint64_t fs_base;
	uint64_t gs_base;
	uint64_t ds;
	uint64_t es;
	uint64_t fs;
	uint64_t gs;
};

template<class Callback, class Timer>
Sandbox::ExitStat Sandbox::Impl<Callback, Timer>::execute(
	const std::string& exec, const std::vector<std::string>& args,
	const Options& opts, const std::string& working_dir, Callback func)
	noexcept(false)
{
	static_assert(std::is_base_of<CallbackBase, Callback>::value,
		"Callback has to derive from Sandbox::CallbackBase");

	// Error stream from tracee (and wait_for_syscall()) via pipe
	int pfd[2];
	if (pipe2(pfd, O_CLOEXEC) == -1)
		THROW("pipe()", error(errno));

	int cpid = fork();
	if (cpid == -1)
		THROW("fork()", error(errno));

	else if (cpid == 0) {
		sclose(pfd[0]);
		runChild(exec, args, opts, working_dir, pfd[1], [&]{
			if (ptrace(PTRACE_TRACEME, 0, 0, 0))
				sendErrorMessage(pfd[1], "ptrace(PTRACE_TRACEME");
		});
	}

	sclose(pfd[1]);
	Closer close_pipe0(pfd[0]);

	// Wait for tracee to be ready
	int status;
	waitpid(cpid, &status, 0);
	// If something went wrong
	if (WIFEXITED(status) || WIFSIGNALED(status))
		return ExitStat(status, 0, receiveErrorMessage(status, pfd[0]));

#ifndef PTRACE_O_EXITKILL
# define PTRACE_O_EXITKILL 0
#endif
	if (ptrace(PTRACE_SETOPTIONS, cpid, 0, PTRACE_O_TRACESYSGOOD
		| PTRACE_O_EXITKILL))
	{
		int ec = errno;
		kill(cpid, SIGKILL);
		waitpid(cpid, nullptr, 0);
		THROW("ptrace(PTRACE_SETOPTIONS)", error(ec));
	}

	// Set up timer
	Timer timer(cpid, opts.time_limit);

	auto wait_for_syscall = [&]() -> int {
		for (;;) {
			(void)ptrace(PTRACE_SYSCALL, cpid, 0, 0); // Fail indicates that
			                                          // the tracee has just
			                                          // died
			waitpid(cpid, &status, 0);

			if (WIFSTOPPED(status)) {
				switch (WSTOPSIG(status)) {
				case SIGTRAP | 0x80:
					return 0; // We are in a syscall

				case SIGSTOP:
				case SIGTRAP:
				case SIGCONT:
					break;

				default:
					// Deliver killing signal to tracee
					ptrace(PTRACE_CONT, cpid, 0, WSTOPSIG(status));
				}
			}

			if (WIFEXITED(status) || WIFSIGNALED(status))
				return -1; // Tracee is dead now
		}
	};

	for (;;) {
		auto exit_normally = [&]() -> ExitStat {
			uint64_t runtime = timer.stopAndGetRuntime();
			if (status)
				return ExitStat(status, runtime,
					receiveErrorMessage(status, pfd[0]));

			return ExitStat(status, runtime, "");
		};

		// Into syscall
		if (wait_for_syscall())
			return exit_normally();

#ifdef __x86_64__
		long syscall = ptrace(PTRACE_PEEKUSER, cpid,
			offsetof(x86_64_user_regs_struct, orig_rax), 0);
#else
		long syscall = ptrace(PTRACE_PEEKUSER, cpid,
			offsetof(i386_user_regs_struct, orig_eax), 0);
#endif

		// If syscall is not allowed
		if (syscall < 0 || !func(cpid, syscall)) {
			uint64_t runtime = timer.stopAndGetRuntime();

			// Kill tracee
			kill(cpid, SIGKILL);
			waitpid(cpid, &status, 0);

			// If time limit exceeded
			if (runtime >= opts.time_limit)
				return ExitStat(status, runtime);

			if (syscall < 0)
				THROW("failed to get syscall - ptrace(): ", toString(syscall),
					error(errno));

			return ExitStat(status, runtime,
				concat("forbidden syscall: ", toString(syscall)));
		}

		// syscall returns
		if (wait_for_syscall())
			return exit_normally();
	}
}
