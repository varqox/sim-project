#pragma once

#include "filesystem.h"
#include "logger.h"

#include <cstddef>
#include <mutex>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>

class Sandbox {
public:
	Sandbox() = delete;

	struct i386_user_regs_struct {
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

	struct x86_64_user_regs_struct {
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

	struct ExitStat {
		int code;
		uint64_t runtime; // in usec
		std::string message;

		explicit ExitStat(int c = 0, uint64_t r = 0, const std::string& m = "")
			: code(c), runtime(r), message(m) {}

		ExitStat(const ExitStat& es)
			: code(es.code), runtime(es.runtime), message(es.message) {}

		ExitStat(ExitStat&& es)
			: code(es.code), runtime(es.runtime),
			message(std::move(es.message)) {}

		ExitStat& operator=(const ExitStat& es) {
			code = es.code;
			runtime = es.runtime;
			message = es.message;
			return *this;
		}

		ExitStat& operator=(ExitStat&& es) {
			code = es.code;
			runtime = es.runtime;
			message = std::move(es.message);
			return *this;
		}
	};

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
		bool allowedCall(pid_t pid, int arch, int syscall,
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

		virtual ~CallbackBase() {}
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

	struct Options {
		uint64_t time_limit; // in usec (0 - disable time limit)
		uint64_t memory_limit; // in bytes (0 - disable memory limit)
		int new_stdin_fd; // negative - close, STDIN_FILENO - do not change
		int new_stdout_fd; // negative - close, STDOUT_FILENO - do not change
		int new_stderr_fd; // negative - close, STDERR_FILENO - do not change

		Options(uint64_t tl, uint64_t ml, int ifd = STDIN_FILENO,
				int ofd = STDOUT_FILENO, int efd = STDERR_FILENO)
			: time_limit(tl), memory_limit(ml), new_stdin_fd(ifd),
				new_stdout_fd(ofd), new_stderr_fd(efd) {}

	};

	/**
	 * @brief Runs @p exec with arguments @p args with limits @p opts.time_limit
	 *   and @p opts.memory_limit under ptrace(2)
	 * @details Callback object is called on every syscall entry called by exec
	 *   with parameters: child pid, syscall number.
	 *   Callback::operator() must return whether syscall is allowed or not
	 *   @p exec is called via execvp()
	 *   This function is thread-safe
	 *
	 * @param exec file that is to be executed
	 * @param args arguments passed to exec
	 * @param opts options (time_limit set to 0 disables time limit,
	 *   memory_limit set to 0 disables memory limit,
	 *   new_stdin_fd, new_stdout_fd, new_stderr_fd - file descriptors to which
	 *   respectively stdin, stdout, stderr of sandboxed process will be changed
	 *   or if negative, closed)
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
		Callback func = Callback())
	{
		static_assert(std::is_base_of<CallbackBase, Callback>::value,
			"Callback has to derive from Sandbox::CallbackBase");

		static std::mutex lock;
		// Only one running sandbox per process is allowed. That is why when one
		// is running, the next ones is executed in new child process.
		std::unique_lock<std::mutex> ulck(lock, std::defer_lock);
		if (ulck.try_lock())
			return execute<Callback>(exec, args, opts, func);

		int pfd[2];
		if (pipe(pfd) == -1)
			throw std::runtime_error(concat("pipe()", error(errno)));

		pid_t child = fork();
		if (child == -1) {
			throw std::runtime_error(concat("Failed to fork()", error(errno)));

		} else if (child == 0) {
			ExitStat es(-1);
			try {
				es = execute<Callback>(exec, args, opts, func);
			} catch (const std::exception& e) {
				// We cannot allow exception to fly out of this thread
				es.message = e.what();
			}

			writeAll(pfd[1], &es, sizeof(es.code) + sizeof(es.runtime));
			writeAll(pfd[1], es.message.data(), es.message.size());
			_exit(0);
		}

		sclose(pfd[1]);
		Closer pipe_close(pfd[0]);

		ExitStat es;
		readAll(pfd[0], &es, sizeof(es.code) + sizeof(es.runtime));

		std::array<char, 4096> buff;
		int rc;
		while ((rc = read(pfd[0], buff.data(), buff.size())) > 0)
			es.message.append(buff.data(), rc);

		// Throw exception caught in child
		if (es.code == -1)
			throw std::runtime_error(es.message);

		waitpid(child, nullptr, 0);
		return es;
	}

private:
	static int cpid;

	static void handle_timeout(int) {
		kill(cpid, SIGKILL);
	}

	/**
	 * @brief Executes @p exec with arguments @p args with limits @p opts.time_limit
	 *   and @p opts.memory_limit under ptrace(2)
	 * @details Callback object is called on every syscall entry called by exec
	 *   with parameters: child pid, syscall number.
	 *   Callback::operator() must return whether syscall is allowed or not
	 *   @p exec is called via execvp()
	 *   This function is not thread-safe
	 *
	 * @param exec file that is to be executed
	 * @param args arguments passed to exec
	 * @param opts options (time_limit set to 0 disables time limit,
	 *   memory_limit set to 0 disables memory limit,
	 *   new_stdin_fd, new_stdout_fd, new_stderr_fd - file descriptors to which
	 *   respectively stdin, stdout, stderr of sandboxed process will be changed
	 *   or if negative, closed)
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
	template<class Callback>
	static ExitStat execute(const std::string& exec,
		const std::vector<std::string>& args, const Options& opts,
		Callback func)
	{
		static_assert(std::is_base_of<CallbackBase, Callback>::value,
			"Callback has to derive from Sandbox::CallbackBase");

		// Error stream from tracee (and wait_for_syscall()) via pipe
		int pfd[2];
		if (pipe2(pfd, O_CLOEXEC) == -1)
			throw std::runtime_error(concat("pipe()", error(errno)));

		cpid = fork();
		if (cpid == -1)
			throw std::runtime_error(concat("fork()", error(errno)));

		else if (cpid == 0) {
			sclose(pfd[0]);
			// Sends error to tracer via pipe
			auto send_error = [pfd](const char* str) {
				std::string msg = concat(str, error(errno));
				writeAll(pfd[1], msg.data(), msg.size());
				_exit(-1);
			};

			// Convert args
			const size_t len = args.size();
			char *arg[len + 1];
			arg[len] = nullptr;

			for (size_t i = 0; i < len; ++i)
				arg[i] = const_cast<char*>(args[i].c_str());

			// Set virtual memory and stack size limit (to the same value)
			if (opts.memory_limit > 0) {
				struct rlimit limit;
				limit.rlim_max = limit.rlim_cur = opts.memory_limit;
				if (setrlimit(RLIMIT_AS, &limit))
					send_error("setrlimit(RLIMIT_AS)");
				if (setrlimit(RLIMIT_STACK, &limit))
					send_error("setrlimit(RLIMIT_STACK)");
			}

			// Change stdin
			if (opts.new_stdin_fd < 0)
				sclose(STDIN_FILENO);

			else if (opts.new_stdin_fd != STDIN_FILENO)
				while (dup2(opts.new_stdin_fd, STDIN_FILENO) == -1)
					if (errno != EINTR)
						_exit(-1);

			// Change stdout
			if (opts.new_stdout_fd < 0)
				sclose(STDOUT_FILENO);

			else if (opts.new_stdout_fd != STDOUT_FILENO)
				while (dup2(opts.new_stdout_fd, STDOUT_FILENO) == -1)
					if (errno != EINTR)
						_exit(-1);

			// Change stderr
			if (opts.new_stderr_fd < 0)
				sclose(STDERR_FILENO);

			else if (opts.new_stderr_fd != STDERR_FILENO)
				while (dup2(opts.new_stderr_fd, STDERR_FILENO) == -1)
					if (errno != EINTR)
						_exit(-1);

			if (ptrace(PTRACE_TRACEME, 0, 0, 0))
				send_error("ptrace(PTRACE_TRACEME)");

			// Signal parent process that tracee is ready to be ptraced
			kill(getpid(), SIGSTOP);

			execv(exec.c_str(), arg);

			send_error("execv()"); // execv() failed
		}

		sclose(pfd[1]);
		Closer close_pipe0(pfd[0]);

		// Wait for tracee to be ready
		int status;
		waitpid(cpid, &status, 0);

#ifndef PTRACE_O_EXITKILL
# define PTRACE_O_EXITKILL 0
#endif
		if (ptrace(PTRACE_SETOPTIONS, cpid, 0, PTRACE_O_TRACESYSGOOD
			| PTRACE_O_EXITKILL))
		{
			int ec = errno;
			kill(cpid, SIGKILL);
			waitpid(cpid, nullptr, 0);
			throw std::runtime_error(concat("ptrace(PTRACE_SETOPTIONS)",
				error(ec)));
		}

		// Set up timer
		struct itimerval timer, old_timer;
		memset(&timer, 0, sizeof(timer));

		timer.it_value.tv_sec = opts.time_limit / 1000000;
		timer.it_value.tv_usec = opts.time_limit - timer.it_value.tv_sec *
			1000000LL;

		// Set timer timeout handler
		struct sigaction sa, sa_old;
		memset (&sa, 0, sizeof(sa));
		sa.sa_handler = &handle_timeout;
		sa.sa_flags = SA_RESTART;
		(void)sigaction(SIGALRM, &sa, &sa_old);

		// Run timer (time limit)
		unsigned long long runtime;
		(void)setitimer(ITIMER_REAL, &timer, &old_timer);

		auto cleanup = [&]() {
			// Disable timer
			(void)setitimer(ITIMER_REAL, &old_timer, &timer);
			(void)sigaction(SIGALRM, &sa_old, nullptr);

			runtime = opts.time_limit - timer.it_value.tv_sec * 1000000LL -
				timer.it_value.tv_usec;
		};

		auto wait_for_syscall = [&status, pfd, &cleanup]() -> int {
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
			// Into syscall
			if (wait_for_syscall()) {
			 exit_normally:
				cleanup();

				std::string message;
				if (status) {
					std::array<char, 4096> buff;
					// Read errors from pipe
					ssize_t rc;
					while ((rc = read(pfd[0], buff.data(), buff.size())) > 0)
						message.append(buff.data(), rc);

					if (message.size()) // Error in tracee
						throw std::runtime_error(message);

					else if (WIFEXITED(status))
						message = concat("returned ",
							toString(WEXITSTATUS(status)));

					else if (WIFSIGNALED(status))
						message = concat("killed by signal ",
							toString(WTERMSIG(status)), " - ",
							strsignal(WTERMSIG(status)));

					else if (WIFSTOPPED(status))
						message = concat("killed by signal ",
							toString(WSTOPSIG(status)), " - ",
							strsignal(WSTOPSIG(status)));
				}

				return ExitStat(status, runtime, message);
			}

#ifdef __x86_64__
			long syscall = ptrace(PTRACE_PEEKUSER, cpid,
				offsetof(x86_64_user_regs_struct, orig_rax), 0);
#else
			long syscall = ptrace(PTRACE_PEEKUSER, cpid,
				offsetof(i386_user_regs_struct, orig_eax), 0);
#endif

			// If syscall is not allowed
			if (syscall < 0 || !func(cpid, syscall)) {
				cleanup();

				// Kill tracee
				kill(cpid, SIGKILL);
				waitpid(cpid, &status, 0);

				// If time limit exceeded
				if (runtime >= opts.time_limit)
					return ExitStat(status, runtime);

				if (syscall < 0)
					throw std::runtime_error(concat("failed to get syscall - "
						"ptrace(): ", toString(syscall), error(errno)));

				return ExitStat(status, runtime,
					concat("forbidden syscall: ", toString(syscall)));
			}

			// syscall returns
			if (wait_for_syscall())
				goto exit_normally;
		}
	}
};
