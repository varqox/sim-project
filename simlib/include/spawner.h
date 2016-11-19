#pragma once

#include "debug.h"
#include "filesystem.h"

#include <mutex>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>

class Spawner {
public:
	Spawner() = delete;

	struct ExitStat {
		int code = 0;
		uint64_t runtime = 0; // in usec
		uint64_t vm_peak = 0; // peak virtual memory size (in bytes)
		std::string message;

		ExitStat() = default;

		ExitStat(int c, uint64_t rt, uint64_t vp, const std::string& msg = "")
			: code(c), runtime(rt), vm_peak(vp), message(msg) {}

		ExitStat(const ExitStat&) = default;
		ExitStat(ExitStat&&) noexcept = default;
		ExitStat& operator=(const ExitStat&) = default;
		ExitStat& operator=(ExitStat&&) = default;
	};

	struct Options {
		int new_stdin_fd; // negative - close, STDIN_FILENO - do not change
		int new_stdout_fd; // negative - close, STDOUT_FILENO - do not change
		int new_stderr_fd; // negative - close, STDERR_FILENO - do not change
		uint64_t time_limit; // in usec (0 - disable time limit)
		uint64_t memory_limit; // in bytes (0 - disable memory limit)

		constexpr Options()
			: Options(STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO, 0, 0) {}

		constexpr Options(int ifd, int ofd, int efd, uint64_t tl = 0,
				uint64_t ml = 0)
			: new_stdin_fd(ifd), new_stdout_fd(ofd), new_stderr_fd(efd),
				time_limit(tl), memory_limit(ml) {}
	};

	/**
	 * @brief Runs @p exec with arguments @p args with limits @p opts.time_limit
	 *   and @p opts.memory_limit
	 * @details @p exec is called via execvp()
	 *   This function is thread-safe
	 *
	 * @param exec filename that is to be executed
	 * @param args arguments passed to exec
	 * @param opts options (new_stdin_fd, new_stdout_fd, new_stderr_fd - file
	 *   descriptors to which respectively stdin, stdout, stderr of spawned
	 *   process will be changed or if negative, closed;
	 *   time_limit set to 0 disables time limit;
	 *   memory_limit set to 0 disables memory limit)
	 * @param working_dir directory at which exec will be run
	 *
	 * @return Returns ExitStat structure with fields:
	 *   - code: return status (in the format specified in wait(2)).
	 *   - runtime: in microseconds [usec]
	 *   - vm_peak: peak virtual memory size [bytes] (ignored - always 0)
	 *   - message: detailed info about error, etc.
	 *
	 * @errors Throws an exception std::runtime_error with appropriate
	 *   information if any syscall fails
	 */
	static ExitStat run(const CStringView& exec,
		const std::vector<std::string>& args, const Options& opts = Options(),
		const CStringView& working_dir = ".")
	{
		return runWithTimer<Impl>(opts.time_limit, exec, args, opts,
			working_dir);
	}

protected:
	// Sends @p str followed by error message through @p fd and _exits
	static void sendErrorMessage(int fd, const char* str) {
		std::string msg = concat(str, error(errno));
		writeAll(fd, msg.data(), msg.size());
		_exit(-1);
	};

	/**
	 * @brief Receives error message from @p fd
	 * @details Useful in conjunction with sendErrorMessage() and pipe for
	 *   instance in reporting errors from child process
	 *
	 * @param status status from waitpid(2)
	 * @param fd file descriptor to read from
	 *
	 * @return the message received
	 */
	static std::string receiveErrorMessage(int status, int fd);

	/**
	 * @brief Initializes child process which will execute @p exec
	 * @details Sets limits and file descriptors specified in opts
	 *
	 * @param exec filename that is to be executed
	 * @param args arguments passed to exec
	 * @param opts options (new_stdin_fd, new_stdout_fd, new_stderr_fd - file
	 *   descriptors to which respectively stdin, stdout, stderr of spawned
	 *   process will be changed or if negative, closed;
	 *   time_limit set to 0 disables time limit;
	 *   memory_limit set to 0 disables memory limit)
	 * @param working_dir directory at which exec will be run
	 * @param fd file descriptor to which errors will be written
	 * @param doBeforeExec function that is to be called before executing
	 *   @p exec
	 */
	template<class Func>
	static void runChild(const CStringView& exec,
		const std::vector<std::string>& args, const Options& opts,
		const CStringView& working_dir, int fd,
		Func doBeforeExec) noexcept;

	/**
	 * @brief This function helps with making thread-unsafe function thread-safe
	 * @details Expects class Func to have static execute(...) method,
	 *   which will be called with proper timer, see e.g. Spawner::Impl
	 *
	 * @param time_limit time_limit used to decide which timer to use
	 * @param args arguments to Func<>::execute()
	 * @tparam class Func structure with static method execute()
	 * @tparam TArgs additional template arguments used during template
	 *   specialization of struct Func
	 * @return return value of Func<>::execute()
	 */
	template<template<class...> class Func, class... TArgs, class... Args>
	static ExitStat runWithTimer(uint64_t time_limit, Args&&... args);

private:
	class ZeroTimer {
		std::chrono::steady_clock::time_point begin;

	public:
		ZeroTimer(int, uint64_t);

		uint64_t stopAndGetRuntime();
	};

	class NormalTimer {
		uint64_t tl;
		struct itimerval timer, old_timer;
		struct sigaction sa_old;

		void stop();

		static int cpid;

		static void handle_timeout(int) { kill(-cpid, SIGKILL); }

	public:
		static std::mutex lock;

		NormalTimer(int pid, uint64_t time_limit);

		uint64_t stopAndGetRuntime();

		~NormalTimer() { stop(); }
	};

	template<class Timer>
	struct Impl {
		/**
		 * @brief Runs @p exec with arguments @p args with limits
		 *   @p opts.time_limit and @p opts.memory_limit
		 * @details @p exec is called via execvp()
		 *   This function is thread-safe
		 *
		 * @param exec filename that is to be executed
		 * @param args arguments passed to exec
		 * @param opts options (new_stdin_fd, new_stdout_fd, new_stderr_fd -
		 *   - file descriptors to which respectively stdin, stdout, stderr of
		 *   spawned process will be changed or if negative, closed;
		 *   time_limit set to 0 disables time limit;
		 *   memory_limit set to 0 disables memory limit)
		 * @param working_dir directory at which exec will be run
		 *
		 * @return Returns ExitStat structure with fields:
		 *   - code: return status (in the format specified in wait(2)).
		 *   - runtime: in microseconds [usec]
		 *   - vm_peak: peak virtual memory size [bytes] (ignored - always 0)
		 *   - message: detailed info about error, etc.
		 *
		 * @errors Throws an exception std::runtime_error with appropriate
		 *   information if any syscall fails
		 */
		static ExitStat execute(const CStringView& exec,
			const std::vector<std::string>& args, const Options& opts,
			const CStringView& working_dir);
	};
};

/******************************* IMPLEMENTATION *******************************/

inline Spawner::ZeroTimer::ZeroTimer(int, uint64_t)
	: begin(std::chrono::steady_clock::now()) {}

inline uint64_t Spawner::ZeroTimer::stopAndGetRuntime() {
	using namespace std::chrono;
	return duration_cast<microseconds>(steady_clock::now() - begin).count();
}

inline void Spawner::NormalTimer::stop() {
	if (tl > 0) {
		// Disable timer
		(void)setitimer(ITIMER_REAL, &old_timer, &timer);
		// Unset timeout handler
		(void)sigaction(SIGALRM, &sa_old, nullptr);
	}
}

inline Spawner::NormalTimer::NormalTimer(int pid, uint64_t time_limit)
	: tl(time_limit), timer({{0, 0}, {0, 0}})
{
	cpid = pid;
	// Initialize new timer
	timer.it_value.tv_sec = tl / 1000000;
	timer.it_value.tv_usec = tl - timer.it_value.tv_sec * 1000000LL;

	// Set timer timeout handler
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &handle_timeout;
	sa.sa_flags = SA_RESTART;
	(void)sigaction(SIGALRM, &sa, &sa_old);

	// Run timer
	(void)setitimer(ITIMER_REAL, &timer, &old_timer);
}

inline uint64_t Spawner::NormalTimer::stopAndGetRuntime() {
	stop();
	uint64_t res = tl - timer.it_value.tv_sec * 1000000LL -
		timer.it_value.tv_usec;
	tl = 0; // Mark that timer was stopped
	return res;
}

template<class Func>
void Spawner::runChild(const CStringView& exec,
	const std::vector<std::string>& args, const Options& opts,
	const CStringView& working_dir, int fd, Func doBeforeExec) noexcept
{
	// Sends error to parent
	auto send_error = [fd](const char* str) { sendErrorMessage(fd, str); };

	// Convert args
	const size_t len = args.size();
	const char* arg[len + 1];
	arg[len] = nullptr;

	for (size_t i = 0; i < len; ++i)
		arg[i] = args[i].c_str();

	// Change working directory
	if (working_dir != "." && working_dir != "" && working_dir != "./") {
		if (chdir(working_dir.c_str()) == -1)
			send_error("chdir()");
	}

	// Create new process group (useful for killing the whole process group)
	if (setpgid(0, 0))
		send_error("setpgid()");

	// Set virtual memory and stack size limit (to the same value)
	if (opts.memory_limit > 0) {
		struct rlimit limit;
		limit.rlim_max = limit.rlim_cur = opts.memory_limit;
		if (setrlimit(RLIMIT_AS, &limit))
			send_error("setrlimit(RLIMIT_AS)");
		if (setrlimit(RLIMIT_STACK, &limit))
			send_error("setrlimit(RLIMIT_STACK)");
	}

	// Set CPU time limit [s] (useful when spawned process becomes orphaned)
	if (opts.time_limit > 0) {
		struct rlimit limit;
		limit.rlim_max = limit.rlim_cur =
			(opts.time_limit + 500000) / 1000000 + 1; // +1 to avoid premature
			                                          // death
		if (setrlimit(RLIMIT_CPU, &limit))
			send_error("setrlimit(RLIMIT_CPU)");
	}

	// Change stdin
	if (opts.new_stdin_fd < 0)
		sclose(STDIN_FILENO);

	else if (opts.new_stdin_fd != STDIN_FILENO)
		while (dup2(opts.new_stdin_fd, STDIN_FILENO) == -1)
			if (errno != EINTR)
				send_error("dup2()");

	// Change stdout
	if (opts.new_stdout_fd < 0)
		sclose(STDOUT_FILENO);

	else if (opts.new_stdout_fd != STDOUT_FILENO)
		while (dup2(opts.new_stdout_fd, STDOUT_FILENO) == -1)
			if (errno != EINTR)
				send_error("dup2()");

	// Change stderr
	if (opts.new_stderr_fd < 0)
		sclose(STDERR_FILENO);

	else if (opts.new_stderr_fd != STDERR_FILENO)
		while (dup2(opts.new_stderr_fd, STDERR_FILENO) == -1)
			if (errno != EINTR)
				send_error("dup2()");

	doBeforeExec();

	// Signal parent process that child is ready to execute @p exec
	kill(getpid(), SIGSTOP);

	execvp(exec.c_str(), (char** const)arg);

	send_error(concat("execvp('", exec, "')").c_str()); // execvp() failed
}

template<template<class...> class Func, class... TArgs, class... Args>
Spawner::ExitStat Spawner::runWithTimer(uint64_t time_limit, Args&&... args) {
	// Without time_limit (NormalTimer) we can run as many processes as we want
	if (time_limit == 0)
		return Func<TArgs..., ZeroTimer>::execute(std::forward<Args>(args)...);

	// Only one running Timer per process is allowed. That is why when one
	// is running, the next ones are executed in the new child process.
	std::unique_lock<std::mutex> ulck(NormalTimer::lock, std::defer_lock);
	if (ulck.try_lock())
		return Func<TArgs..., NormalTimer>::execute(std::forward<Args>(args)...);

	// There is already one Timer running in this process, so move to child
	int pfd[2];
	if (pipe(pfd) == -1)
		THROW("pipe()", error(errno));

	pid_t child = fork();
	if (child == -1) {
		THROW("Failed to fork()", error(errno));

	} else if (child == 0) {
		ExitStat es;
		es.code = -1; // If not overwritten, it will indicate an error -
		              // caught exception
		try {
			es = Func<TArgs..., NormalTimer>::execute(
				std::forward<Args>(args)...);
		} catch (const std::exception& e) {
			// We cannot allow exception to fly out of this thread
			es.message = e.what();
		}

		writeAll(pfd[1], &es, sizeof(es.code) + sizeof(es.runtime) +
			sizeof(es.vm_peak));
		writeAll(pfd[1], es.message.data(), es.message.size());
		_exit(0);
	}

	sclose(pfd[1]);
	Closer pipe0_closer(pfd[0]);

	ExitStat es; // Loaded from message from child
	readAll(pfd[0], &es, sizeof(es.code) + sizeof(es.runtime) +
		sizeof(es.vm_peak));

	std::array<char, 4096> buff;
	int rc;
	while ((rc = read(pfd[0], buff.data(), buff.size())) > 0)
		es.message.append(buff.data(), rc);

	waitpid(child, nullptr, 0); // No status, because child is not a spawned
	                            // process

	// Throw exception caught in child
	if (es.code == -1)
		THROW(es.message);
	return es;
}

template<class Timer>
Spawner::ExitStat Spawner::Impl<Timer>::execute(const CStringView& exec,
	const std::vector<std::string>& args, const Spawner::Options& opts,
	const CStringView& working_dir)
{
	// Error stream from child (and wait_for_syscall()) via pipe
	int pfd[2];
	if (pipe2(pfd, O_CLOEXEC) == -1)
		THROW("pipe()", error(errno));

	int cpid = fork();
	if (cpid == -1)
		THROW("fork()", error(errno));

	else if (cpid == 0) {
		sclose(pfd[0]);
		runChild(exec, args, opts, working_dir, pfd[1], []{});
	}

	sclose(pfd[1]);
	Closer pipe0_closer(pfd[0]);

	// Wait for child to be ready
	int status;
	waitpid(cpid, &status, WUNTRACED);
	// If something went wrong
	if (WIFEXITED(status) || WIFSIGNALED(status))
		return ExitStat(status, 0, 0, receiveErrorMessage(status, pfd[0]));

	// Set up timer
	Timer timer(cpid, opts.time_limit);
	kill(cpid, SIGCONT);

	// Wait for death of the child
	do {
		waitpid(cpid, &status, 0);
	} while (!WIFEXITED(status) && !WIFSIGNALED(status));

	uint64_t runtime = timer.stopAndGetRuntime();
	if (status)
		return ExitStat(status, runtime, 0,
			receiveErrorMessage(status, pfd[0]));

	return ExitStat(status, runtime, 0, "");
}
