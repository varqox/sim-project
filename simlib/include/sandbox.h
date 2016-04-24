#pragma once

#include "process.h"
#include "utilities.h"

#include <cstddef>
#include <sys/ptrace.h>

class Sandbox : protected Spawner {
public:
	Sandbox() = delete;

	struct i386_user_regset;

	struct x86_64_user_regset;

	class CallbackBase {
	protected:
		/**
		 * @brief Checks whether syscall open(2) is allowed, if not, tries to
		 *   modify it (by replacing filename with nullptr) so that syscall will
		 *   fail
		 *
		 * @param pid pid of traced process (via ptrace)
		 * @param arch architecture: 0 - i386, 1 - x86_64
		 * @param allowed_filed files which are allowed to be opened
		 *
		 * @return true if call is allowed (modified if needed), false otherwise
		 */
		bool isSysOpenAllowed(pid_t pid, int arch,
			const std::vector<std::string>& allowed_files = {});

	public:
		/**
		 * @brief Checks whether or not entering syscall @p syscall is allowed
		 *
		 * @param pid sandboxed process id
		 * @param syscall executed syscall number to check
		 *
		 * @return true if syscall is allowed to be executed, false otherwise
		 */
		virtual bool isSyscallEntryAllowed(pid_t pid, int syscall) = 0;

		/**
		 * @brief Checks whether or not exit from finished syscall @p syscall
		 *   is allowed
		 *
		 * @param pid sandboxed process id
		 * @param syscall executed syscall number to check
		 *
		 * @return true if finished syscall is allowed to exit, false otherwise
		 */
		virtual bool isSyscallExitAllowed(pid_t pid, int syscall) = 0;

		/**
		 * @brief Returns error message which was set after unsuccessful call to
		 *   either isSyscallEntryAllowed() or isSyscallExitAllowed()
		 * @return an error message
		 */
		virtual std::string errorMessage() const = 0;

		virtual ~CallbackBase() noexcept {}
	};

	class DefaultCallback : public CallbackBase {
	protected:
		struct Pair {
			int syscall;
			int limit;
		};

		int8_t counter = 0; // number of early isSyscallEntryAllowed() calls
		int8_t arch = -1; // arch - architecture: 0 - i386, 1 - x86_64
		static_assert(ARCH_i386 == 0 && ARCH_x86_64 == 1,
			"Invalid values of ARCH_ constants");
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

		int unsuccessful_SYS_brk_counter = 0; // used in isSyscallExitAllowed()
		static constexpr int UNSUCCESSFUL_SYS_BRK_LIMIT = 128;

		std::string error_message;

		template<size_t N1, size_t N2>
		bool isSyscallEntryAllowed(pid_t pid, int syscall,
			const std::array<int, N1>& allowed_syscalls_i386,
			const std::array<int, N2>& allowed_syscalls_x86_64,
			const std::vector<std::string>& allowed_files);

	public:
		DefaultCallback() = default;

		virtual ~DefaultCallback() = default;

		bool isSyscallEntryAllowed(pid_t pid, int syscall) {
			constexpr std::array<int, 20> allowed_syscalls_i386 {{
				1, // SYS_exit
				3, // SYS_read
				4, // SYS_write
				6, // SYS_close
				13, // SYS_time
				45, // SYS_brk
				54, // SYS_ioctl
				90, // SYS_mmap
				91, // SYS_munmap
				108, // SYS_fstat
				125, // SYS_mprotect
				145, // SYS_readv
				146, // SYS_writev
				174, // SYS_rt_sigaction
				175, // SYS_rt_sigprocmask
				192, // SYS_mmap2
				197, // SYS_fstat64
				224, // SYS_gettid
				252, // SYS_exit_group
				270, // SYS_tgkill
			}};
			constexpr std::array<int, 18> allowed_syscalls_x86_64 {{
				0, // SYS_read
				1, // SYS_write
				3, // SYS_close
				5, // SYS_fstat
				9, // SYS_mmap
				10, // SYS_mprotect
				11, // SYS_munmap
				12, // SYS_brk
				13, // SYS_rt_sigaction
				14, // SYS_rt_sigprocmask
				16, // SYS_ioctl
				19, // SYS_readv
				20, // SYS_writev
				60, // SYS_exit
				186, // SYS_gettid
				201, // SYS_time
				231, // SYS_exit_group
				234, // SYS_tgkill
			}};

			return isSyscallEntryAllowed(pid, syscall, allowed_syscalls_i386,
				allowed_syscalls_x86_64, {});
		}

		bool isSyscallExitAllowed(pid_t pid, int syscall);

		std::string errorMessage() const { return error_message; }
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

#if 0
# warning "Before committing disable this debug"
# define DEBUG_SANDBOX(...) __VA_ARGS__
#else
# define DEBUG_SANDBOX(...)
#endif

struct Sandbox::i386_user_regset {
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

struct Sandbox::x86_64_user_regset {
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

template<size_t N1, size_t N2>
bool Sandbox::DefaultCallback::isSyscallEntryAllowed(pid_t pid, int syscall,
	const std::array<int, N1>& allowed_syscalls_i386,
	const std::array<int, N2>& allowed_syscalls_x86_64,
	const std::vector<std::string>& allowed_files)
{
	// Detect arch (first call - before exec, second - after exec)
	if (counter < 2) {
		arch = detectArchitecture(pid);
		++counter;
	}

	// Check if syscall is allowed
	if (arch == ARCH_i386) {
		if (binary_search(allowed_syscalls_i386, syscall))
			return true;
	} else {
		if (binary_search(allowed_syscalls_x86_64, syscall))
			return true;
	}

	// Check if syscall is limited
	for (Pair& i : limited_syscalls[arch])
		if (syscall == i.syscall)
			return (--i.limit >= 0);

	constexpr int sys_open[2] = {
		5, // SYS_open - i386
		2 // SYS_open - x86_64
	};
	if (syscall == sys_open[arch])
		return isSysOpenAllowed(pid, arch, allowed_files);

	return false;
}

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
			offsetof(x86_64_user_regset, orig_rax), 0);
#else
		long syscall = ptrace(PTRACE_PEEKUSER, cpid,
			offsetof(i386_user_regset, orig_eax), 0);
#endif

		// If syscall entry is allowed
		if (syscall >= 0 && func.isSyscallEntryAllowed(cpid, syscall)) {
			// Syscall returns
			if (wait_for_syscall())
				return exit_normally();

			DEBUG_SANDBOX(
#ifdef __x86_64__
				long ret_val = ptrace(PTRACE_PEEKUSER, cpid,
					offsetof(x86_64_user_regset, rax), 0);
				long arg1 = ptrace(PTRACE_PEEKUSER, cpid,
					offsetof(x86_64_user_regset, rdi), 0);
				long arg2 = ptrace(PTRACE_PEEKUSER, cpid,
					offsetof(x86_64_user_regset, rsi), 0);
#else
				long ret_val = ptrace(PTRACE_PEEKUSER, cpid,
					offsetof(i386_user_regset, eax), 0);
				long arg1 = ptrace(PTRACE_PEEKUSER, cpid,
					offsetof(i386_user_regset, ebx), 0);
				long arg2 = ptrace(PTRACE_PEEKUSER, cpid,
					offsetof(i386_user_regset, ecx), 0);
#endif
				stdlog("syscall: ", toString(syscall), '(',
					toString(arg1), ", ", toString(arg2), ", ...) -> ",
					toString(ret_val));
			)

			if (func.isSyscallExitAllowed(cpid, syscall))
				continue;
		}

		/* Syscall entry or exit is not allowed or syscall < 0 */
		uint64_t runtime = timer.stopAndGetRuntime();

		// Kill tracee
		kill(cpid, SIGKILL);
		waitpid(cpid, &status, 0);

		// If time limit was exceeded
		if (runtime >= opts.time_limit)
			return ExitStat(status, runtime, "Time limit exceeded");

		if (syscall < 0)
			THROW("failed to get syscall - ptrace(): ", toString(syscall),
				error(errno));

		std::string message = func.errorMessage();
		if (message.empty())
			message =  concat("forbidden syscall: ", toString(syscall));
		return ExitStat(status, runtime, message);
	}
}
