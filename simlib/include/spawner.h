#pragma once

#include "filesystem.h"
#include "time.h"

#include <sys/resource.h>
#include <sys/wait.h>

class Spawner {
public:
	Spawner() = delete;

	struct ExitStat {
		timespec runtime = {0, 0};
		struct {
			int code; // si_code field from siginfo_t from waitid(2)
			int status; // si_status field from siginfo_t from waitid(2)
		} si {};
		struct rusage rusage = {}; // resource information
		uint64_t vm_peak = 0; // peak virtual memory size (in bytes)
		std::string message;

		ExitStat() = default;

		ExitStat(timespec rt, int sic, int sis, const struct rusage& rus,
				uint64_t vp, const std::string& msg = "")
			: runtime(rt), si {sic, sis}, rusage(rus), vm_peak(vp),
				message(msg) {}

		ExitStat(const ExitStat&) = default;
		ExitStat(ExitStat&&) noexcept = default;
		ExitStat& operator=(const ExitStat&) = default;
		ExitStat& operator=(ExitStat&&) = default;
	};

	struct Options {
		int new_stdin_fd; // negative - close, STDIN_FILENO - do not change
		int new_stdout_fd; // negative - close, STDOUT_FILENO - do not change
		int new_stderr_fd; // negative - close, STDERR_FILENO - do not change
		timespec real_time_limit; // ({0, 0} - disable real time limit)
		uint64_t memory_limit; // in bytes (0 - disable memory limit)
		uint64_t cpu_time_limit; // in seconds (0 - if the real time limit is
		                         // set then cpu time limit will be set to
		                         // round(real time limit in seconds) + 1)
		                         // seconds

		constexpr Options()
			: Options(STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO) {}

		constexpr Options(int ifd, int ofd, int efd, timespec rtl = {0, 0},
				uint64_t ml = 0, uint64_t ctl = 0)
			: new_stdin_fd(ifd), new_stdout_fd(ofd), new_stderr_fd(efd),
				real_time_limit(rtl), memory_limit(ml), cpu_time_limit(ctl) {}
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
	 *   time_limit set to 0 disables the time limit;
	 *   memory_limit set to 0 disables memory limit)
	 * @param working_dir directory at which exec will be run
	 *
	 * @return Returns ExitStat structure with fields:
	 *   - runtime: in timespec structure {sec, nsec}
	 *   - si: {
	 *       code: si_code form siginfo_t from waitid(2)
	 *       status: si_status form siginfo_t from waitid(2)
	 *     }
	 *   - rusage: resource used (see getrusage(2)).
	 *   - vm_peak: peak virtual memory size [bytes] (ignored - always 0)
	 *   - message: detailed info about error, etc.
	 *
	 * @errors Throws an exception std::runtime_error with appropriate
	 *   information if any syscall fails
	 */
	static ExitStat run(CStringView exec, const std::vector<std::string>& args,
		const Options& opts = Options(),
		CStringView working_dir = CStringView{"."});

protected:
	// Sends @p str followed by error message of @p errnum through @p fd and
	// _exits with -1
	static void sendErrorMessage(int fd, int errnum, CStringView str) noexcept {
		writeAll(fd, str.data(), str.size());

		auto err = error(errnum);
		writeAll(fd, err.data(), err.size());

		_exit(-1);
	};

	/**
	 * @brief Receives error message from @p fd
	 * @details Useful in conjunction with sendErrorMessage() and pipe for
	 *   instance in reporting errors from child process
	 *
	 * @param si sig_info from waitid(2)
	 * @param fd file descriptor to read from
	 *
	 * @return the message received
	 */
	static std::string receiveErrorMessage(const siginfo_t& si, int fd);

	/**
	 * @brief Initializes child process which will execute @p exec, this
	 *   function does not return (it kills the process instead)!
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
	static void runChild(CStringView exec, const std::vector<std::string>& args,
		const Options& opts, CStringView working_dir, int fd, Func doBeforeExec)
		noexcept;

	class Timer {
	private:
		pid_t pid_;
		timespec tlimit;
		// used only if time_limit == {0, 0}
		timespec begin_point;
		// used only if time_limit != {0, 0}
		timer_t timerid;

		static void handle_timeout(int, siginfo_t* si, void*) {
			kill(-*(pid_t*)si->si_value.sival_ptr, SIGKILL);
		}

	public:
		Timer(pid_t pid, timespec time_limit) : pid_ {pid}, tlimit(time_limit) {
			if (tlimit.tv_sec == 0 and tlimit.tv_nsec == 0) {
				if (clock_gettime(CLOCK_MONOTONIC, &begin_point))
					THROW("clock_gettime()", error(errno));

			} else {
				// Install (timeout) signal handler
				struct sigaction sa;
				sa.sa_flags = SA_SIGINFO | SA_RESTART;
				sa.sa_sigaction = handle_timeout;
				if (sigaction(SIGRTMIN, &sa, nullptr))
					THROW("signaction()", error(errno));

				// Prepare timer
				sigevent sev;
				memset(&sev, 0, sizeof(sev));
				sev.sigev_notify = SIGEV_SIGNAL;
				sev.sigev_signo = SIGRTMIN;
				sev.sigev_value.sival_ptr = &pid_;
				if (timer_create(CLOCK_MONOTONIC, &sev, &timerid))
					THROW("timer_create()", error(errno));

				// Arm timer
				itimerspec its {{0, 0}, tlimit};
				if (timer_settime(timerid, 0, &its, nullptr)) {
				int errnum = errno;
					timer_delete(timerid);
					THROW("timer_settime()", error(errnum));
				}
			}
		}

		timespec stop_and_get_runtime() {
			if (tlimit.tv_sec == 0 and tlimit.tv_nsec == 0) {
				timespec end_point;
				if (clock_gettime(CLOCK_MONOTONIC, &end_point))
					THROW("clock_gettime()", error(errno));
				return end_point - begin_point;
			}

			itimerspec its {{0, 0}, {0, 0}}, old;
			if (timer_settime(timerid, 0, &its, &old)) {
				int errnum = errno;
				timer_delete(timerid);
				THROW("timer_settime()", error(errnum));
			}

			timer_delete(timerid);
			return tlimit - old.it_value;
		}
	};
};

/******************************* IMPLEMENTATION *******************************/

template<class Func>
void Spawner::runChild(CStringView exec, const std::vector<std::string>& args,
	const Options& opts, CStringView working_dir, int fd, Func doBeforeExec)
	noexcept
{
	// Sends error to parent
	auto send_error = [fd](int errnum, CStringView str) {
		sendErrorMessage(fd, errnum, str);
	};

	// Convert args
	const size_t len = args.size();
	const char* arg[len + 1];
	arg[len] = nullptr;

	for (size_t i = 0; i < len; ++i)
		arg[i] = args[i].c_str();

	// Change working directory
	if (working_dir != "." && working_dir != "" && working_dir != "./") {
		if (chdir(working_dir.c_str()) == -1)
			send_error(errno, "chdir()");
	}

	// Create new process group (useful for killing the whole process group)
	if (setpgid(0, 0))
		send_error(errno, "setpgid()");

	// Set virtual memory and stack size limit (to the same value)
	if (opts.memory_limit > 0) {
		struct rlimit limit;
		limit.rlim_max = limit.rlim_cur = opts.memory_limit;
		if (setrlimit(RLIMIT_AS, &limit))
			send_error(errno, "setrlimit(RLIMIT_AS)");
		if (setrlimit(RLIMIT_STACK, &limit))
			send_error(errno, "setrlimit(RLIMIT_STACK)");
	}

	// Set CPU time limit [s]
	if (opts.cpu_time_limit > 0) {
		rlimit limit {opts.cpu_time_limit, opts.cpu_time_limit};

		if (setrlimit(RLIMIT_CPU, &limit))
			send_error(errno, "setrlimit(RLIMIT_CPU)");

	// Limit below is useful when spawned process becomes orphaned
	} else if (opts.real_time_limit.tv_sec or opts.real_time_limit.tv_nsec) {
		rlimit limit;
		limit.rlim_max = limit.rlim_cur =
			opts.real_time_limit.tv_sec + 1 + // +1 to avoid premature death
			(opts.real_time_limit.tv_nsec > 500000000);

		if (setrlimit(RLIMIT_CPU, &limit))
			send_error(errno, "setrlimit(RLIMIT_CPU)");
	}

	// Change stdin
	if (opts.new_stdin_fd < 0)
		sclose(STDIN_FILENO);

	else if (opts.new_stdin_fd != STDIN_FILENO)
		while (dup2(opts.new_stdin_fd, STDIN_FILENO) == -1)
			if (errno != EINTR)
				send_error(errno, "dup2()");

	// Change stdout
	if (opts.new_stdout_fd < 0)
		sclose(STDOUT_FILENO);

	else if (opts.new_stdout_fd != STDOUT_FILENO)
		while (dup2(opts.new_stdout_fd, STDOUT_FILENO) == -1)
			if (errno != EINTR)
				send_error(errno, "dup2()");

	// Change stderr
	if (opts.new_stderr_fd < 0)
		sclose(STDERR_FILENO);

	else if (opts.new_stderr_fd != STDERR_FILENO)
		while (dup2(opts.new_stderr_fd, STDERR_FILENO) == -1)
			if (errno != EINTR)
				send_error(errno, "dup2()");

	doBeforeExec();

	// Signal parent process that child is ready to execute @p exec
	kill(getpid(), SIGSTOP);

	execvp(exec.c_str(), (char** const)arg);
	int errnum = errno;

	// execvp() failed
	if (exec.size() <= PATH_MAX)
		send_error(errnum, StringBuff<PATH_MAX + 20>{"execvp('", exec, "')"});
	else
		send_error(errnum, StringBuff<PATH_MAX + 20>{"execvp('",
			exec.substring(0, PATH_MAX), "...')"});
}
