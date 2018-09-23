#pragma once

#include "filesystem.h"
#include "time.h"

#include <sys/resource.h>
#include <sys/wait.h>

class Spawner {
protected:
	Spawner() = default;

public:
	struct ExitStat {
		timespec runtime = {0, 0};
		timespec cpu_runtime = {0, 0};
		struct {
			int code; // si_code field from siginfo_t from waitid(2)
			int status; // si_status field from siginfo_t from waitid(2)
		} si {};
		struct rusage rusage = {}; // resource information
		uint64_t vm_peak = 0; // peak virtual memory size (in bytes)
		std::string message;

		ExitStat() = default;

		ExitStat(timespec rt, timespec cpu_time, int sic, int sis,
				const struct rusage& rus, uint64_t vp,
				const std::string& msg = {})
			: runtime(rt), cpu_runtime(cpu_time), si {sic, sis}, rusage(rus),
				vm_peak(vp), message(msg) {}

		ExitStat(const ExitStat&) = default;
		ExitStat(ExitStat&&) noexcept = default;
		ExitStat& operator=(const ExitStat&) = default;
		ExitStat& operator=(ExitStat&&) = default;
	};

	struct Options {
		int new_stdin_fd; // negative - close, STDIN_FILENO - do not change
		int new_stdout_fd; // negative - close, STDOUT_FILENO - do not change
		int new_stderr_fd; // negative - close, STDERR_FILENO - do not change
		timespec real_time_limit = {0, 0}; // ({0, 0} - disable real time limit)
		uint64_t memory_limit = 0; // in bytes (0 - disable memory limit)
		timespec cpu_time_limit{}; // in nanoseconds (0 - if the real time limit
		                           // is set, then CPU time limit will be set to
		                           // round(real time limit in seconds) + 1)
		                           // seconds
		CStringView working_dir; // directory at which program will be run

		constexpr Options()
			: Options(STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO) {}

		constexpr Options(int ifd, int ofd, int efd, CStringView wd)
			: new_stdin_fd(ifd), new_stdout_fd(ofd), new_stderr_fd(efd),
				working_dir(std::move(wd)) {}

		constexpr Options(int ifd, int ofd, int efd, timespec rtl = {0, 0},
				uint64_t ml = 0, timespec ctl = {},
				CStringView wd = CStringView("."))
			: new_stdin_fd(ifd), new_stdout_fd(ofd), new_stderr_fd(efd),
				real_time_limit(rtl), memory_limit(ml), cpu_time_limit(ctl),
				working_dir(std::move(wd)) {}
	};

	/**
	 * @brief Runs @p exec with arguments @p exec_args and limits:
	 *   @p opts.time_limit and @p opts.memory_limit
	 * @details @p exec is called via execvp()
	 *   This function is thread-safe.
	 *   IMPORTANT: To function properly this function uses internally signals
	 *     SIGRTMIN and SIGRTMIN + 1 and installs handlers for them. So be aware
	 *     that using these signals while this function runs (in any thread) is
	 *     not safe. Moreover if your program installed handler for the above
	 *     signals, it must install them again after the function returns.
	 *
	 *
	 * @param exec path to file will be executed
	 * @param exec_args arguments passed to exec
	 * @param opts options (new_stdin_fd, new_stdout_fd, new_stderr_fd - file
	 *   descriptors to which respectively stdin, stdout, stderr of spawned
	 *   process will be changed or if negative, closed;
	 *   time_limit set to 0 disables the time limit;
	 *   memory_limit set to 0 disables memory limit;
	 *   working_dir set to "", "." or "./" disables changing working directory)
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
	static ExitStat run(FilePath exec,
		const std::vector<std::string>& exec_args,
		const Options& opts = Options());

protected:
	// Sends @p str through @p fd and _exits with -1
	static void send_error_message_and_exit(int fd, CStringView str) noexcept
	{
		writeAll(fd, str.data(), str.size());
		_exit(-1);
	};

	// Sends @p str followed by error message of @p errnum through @p fd and
	// _exits with -1
	static void send_error_message_and_exit(int fd, int errnum, CStringView str) noexcept
	{
		writeAll(fd, str.data(), str.size());

		auto err = errmsg(errnum);
		writeAll(fd, err.data(), err.size());

		_exit(-1);
	};

	/**
	 * @brief Receives error message from @p fd
	 * @details Useful in conjunction with send_error_message_and_exit() and
	 *   pipe for instance in reporting errors from child process
	 *
	 * @param si sig_info from waitid(2)
	 * @param fd file descriptor to read from
	 *
	 * @return the message received
	 */
	static std::string receive_error_message(const siginfo_t& si, int fd);

	/**
	 * @brief Initializes child process which will execute @p exec, this
	 *   function does not return (it kills the process instead)!
	 * @details Sets limits and file descriptors specified in opts.
	 *
	 * @param exec filename that is to be executed
	 * @param exec_args arguments passed to exec
	 * @param opts options (new_stdin_fd, new_stdout_fd, new_stderr_fd - file
	 *   descriptors to which respectively stdin, stdout, stderr of spawned
	 *   process will be changed or if negative, closed;
	 *   time_limit set to 0 disables time limit;
	 *   memory_limit set to 0 disables memory limit;
	 *   working_dir set to "", "." or "./" disables changing working directory)
	 * @param fd file descriptor to which errors will be written
	 * @param doBeforeExec function that is to be called before executing
	 *   @p exec
	 */
	template<class Func>
	static void run_child(FilePath exec,
		const std::vector<std::string>& exec_args, const Options& opts, int fd,
		Func doBeforeExec) noexcept;

	class Timer {
	private:
		pid_t pid_;
		timespec tlimit;
		// used only if time_limit == {0, 0}
		timespec begin_point;
		// used only if time_limit != {0, 0}
		timer_t timerid;
		bool timer_is_active = false;

		static void handle_timeout(int, siginfo_t* si, void*) noexcept {
			int errnum = errno;
			kill(-*(pid_t*)si->si_value.sival_ptr, SIGKILL);
			errno = errnum;
		}

		void delete_timer() noexcept {
			if (timer_is_active) {
				timer_is_active = false;
				(void)timer_delete(timerid);
			}
		}

	public:
		Timer(pid_t pid, timespec time_limit) : pid_ {pid}, tlimit(time_limit) {
			if (tlimit.tv_sec == 0 and tlimit.tv_nsec == 0) {
				if (clock_gettime(CLOCK_MONOTONIC, &begin_point))
					THROW("clock_gettime()", errmsg());

			} else {
				const int USED_SIGNAL = SIGRTMIN;

				// Install (timeout) signal handler
				struct sigaction sa;
				sa.sa_flags = SA_SIGINFO | SA_RESTART;
				sa.sa_sigaction = handle_timeout;
				if (sigaction(USED_SIGNAL, &sa, nullptr))
					THROW("signaction()", errmsg());

				// Prepare timer
				sigevent sev;
				memset(&sev, 0, sizeof(sev));
				sev.sigev_notify = SIGEV_SIGNAL;
				sev.sigev_signo = USED_SIGNAL;
				sev.sigev_value.sival_ptr = &pid_;
				if (timer_create(CLOCK_MONOTONIC, &sev, &timerid))
					THROW("timer_create()", errmsg());

				timer_is_active = true;

				// Arm timer
				itimerspec its {{0, 0}, tlimit};
				if (timer_settime(timerid, 0, &its, nullptr)) {
					int errnum = errno;
					delete_timer();
					THROW("timer_settime()", errmsg(errnum));
				}
			}
		}

		timespec stop_and_get_runtime() {
			if (tlimit.tv_sec == 0 and tlimit.tv_nsec == 0) {
				timespec end_point;
				if (clock_gettime(CLOCK_MONOTONIC, &end_point))
					THROW("clock_gettime()", errmsg());
				return end_point - begin_point;
			}

			itimerspec its {{0, 0}, {0, 0}}, old;
			if (timer_settime(timerid, 0, &its, &old))
				THROW("timer_settime()", errmsg());

			delete_timer();
			return tlimit - old.it_value;
		}

		~Timer() { delete_timer(); }
	};

	class CPUTimeMonitor {
	private:
		struct Data {
			pid_t pid;
			clockid_t cid;
			timespec cpu_abs_time_limit; // Counting from 0, not from cpu_time_at_start
			timespec cpu_time_at_start;
			timer_t timerid;
		} data;
		bool timer_is_active = false;

		static void handler(int, siginfo_t* si, void*) noexcept {
			int errnum = errno;

			Data& data = *(Data*)si->si_value.sival_ptr;
			timespec ts;
			if (clock_gettime(data.cid, &ts) or ts >= data.cpu_abs_time_limit) {
				kill(-data.pid, SIGKILL); // Failed to get time or
				                          // cpu_time_limit expired
			} else {
				// There is still time left
				itimerspec its {
					{0, 0},
					meta::max(data.cpu_abs_time_limit - ts,
						timespec{0, (int)0.01e9}) // Min. wait duration: 0.01 s
				};
				timer_settime(data.timerid, 0, &its, nullptr);
			}

			errno = errnum;
		}

	public:
		CPUTimeMonitor(pid_t pid, timespec cpu_time_limit)
			: data{pid, {}, cpu_time_limit, {}, {}}
		{
			if (clock_getcpuclockid(pid, &data.cid))
				THROW("clock_getcpuclockid()", errmsg());

			if (clock_gettime(data.cid, &data.cpu_time_at_start))
				THROW("clock_gettime()", errmsg());

			if (cpu_time_limit == timespec{0, 0})
				return; // No limit is set - nothing to do

			// Update the cpu_abs_time_limit to reflect cpu_time_at_start
			data.cpu_abs_time_limit += data.cpu_time_at_start;

			const int USED_SIGNAL = SIGRTMIN + 1;

			struct sigaction sa;
			sa.sa_flags = SA_SIGINFO | SA_RESTART;
			sa.sa_sigaction = handler;
			sigfillset(&sa.sa_mask); // Prevent interrupting
			if (sigaction(USED_SIGNAL, &sa, nullptr))
				THROW("sigaction()", errmsg());

			// Prepare timer
			sigevent sev;
			memset(&sev, 0, sizeof(sev));
			sev.sigev_notify = SIGEV_SIGNAL;
			sev.sigev_signo = USED_SIGNAL;
			sev.sigev_value.sival_ptr = &data;
			if (timer_create(CLOCK_MONOTONIC, &sev, &data.timerid))
				THROW("timer_create()", errmsg());

			timer_is_active = true;

			itimerspec its {{0, 0}, cpu_time_limit};
			if (timer_settime(data.timerid, 0, &its, nullptr)) {
				int errnum = errno;
				deactivate();
				THROW("timer_settime()", errmsg(errnum));
			}
		}

		void deactivate() noexcept {
			if (timer_is_active) {
				timer_is_active = false;
				timer_delete(data.timerid);
			}
		}

		timespec get_cpu_runtime() {
			timespec ts;
			if (clock_gettime(data.cid, &ts))
				THROW("clock_gettime()", errmsg());

			return ts - data.cpu_time_at_start;
		}

		~CPUTimeMonitor() { deactivate(); }
	};
};

/******************************* IMPLEMENTATION *******************************/

template<class Func>
void Spawner::run_child(FilePath exec,
	const std::vector<std::string>& exec_args, const Options& opts, int fd,
	Func doBeforeExec) noexcept
{
	// Sends error to parent
	auto send_error_and_exit = [fd](int errnum, CStringView str) {
		send_error_message_and_exit(fd, errnum, str);
	};

	// Create new process group (useful for killing the whole process group)
	if (setpgid(0, 0))
		send_error_and_exit(errno, "setpgid()");

	// Convert exec_args
	const size_t len = exec_args.size();
	const char* args[len + 1];
	args[len] = nullptr;

	for (size_t i = 0; i < len; ++i)
		args[i] = exec_args[i].c_str();

	// Change working directory
	if (not isOneOf(opts.working_dir, "", ".", "./")) {
		if (chdir(opts.working_dir.c_str()) == -1)
			send_error_and_exit(errno, "chdir()");
	}

	// Set virtual memory and stack size limit (to the same value)
	if (opts.memory_limit > 0) {
		struct rlimit limit;
		limit.rlim_max = limit.rlim_cur = opts.memory_limit;
		if (setrlimit(RLIMIT_AS, &limit))
			send_error_and_exit(errno, "setrlimit(RLIMIT_AS)");
		if (setrlimit(RLIMIT_STACK, &limit))
			send_error_and_exit(errno, "setrlimit(RLIMIT_STACK)");
	}

	// Set CPU time limit [s]
	// Limit below is useful when spawned process becomes orphaned
	if (opts.cpu_time_limit > timespec{0, 0}) {
		rlimit limit;
		limit.rlim_cur = limit.rlim_max = opts.cpu_time_limit.tv_sec + 1 +
			(opts.cpu_time_limit.tv_nsec >= 500000000); // + to avoid premature
			                                            // death

		if (setrlimit(RLIMIT_CPU, &limit))
			send_error_and_exit(errno, "setrlimit(RLIMIT_CPU)");

	// Limit below is useful when spawned process becomes orphaned
	} else if (opts.real_time_limit.tv_sec or opts.real_time_limit.tv_nsec) {
		rlimit limit;
		limit.rlim_max = limit.rlim_cur =
			opts.real_time_limit.tv_sec + 1 + // + to avoid premature death
			(opts.real_time_limit.tv_nsec >= 500000000);

		if (setrlimit(RLIMIT_CPU, &limit))
			send_error_and_exit(errno, "setrlimit(RLIMIT_CPU)");
	}

	// Change stdin
	if (opts.new_stdin_fd < 0)
		sclose(STDIN_FILENO);

	else if (opts.new_stdin_fd != STDIN_FILENO)
		while (dup2(opts.new_stdin_fd, STDIN_FILENO) == -1)
			if (errno != EINTR)
				send_error_and_exit(errno, "dup2()");

	// Change stdout
	if (opts.new_stdout_fd < 0)
		sclose(STDOUT_FILENO);

	else if (opts.new_stdout_fd != STDOUT_FILENO)
		while (dup2(opts.new_stdout_fd, STDOUT_FILENO) == -1)
			if (errno != EINTR)
				send_error_and_exit(errno, "dup2()");

	// Change stderr
	if (opts.new_stderr_fd < 0)
		sclose(STDERR_FILENO);

	else if (opts.new_stderr_fd != STDERR_FILENO)
		while (dup2(opts.new_stderr_fd, STDERR_FILENO) == -1)
			if (errno != EINTR)
				send_error_and_exit(errno, "dup2()");

	doBeforeExec();

	// Signal parent process that child is ready to execute @p exec
	kill(getpid(), SIGSTOP);

	execvp(exec, (char** const)args);
	int errnum = errno;

	// execvp() failed
	if (exec.size() <= PATH_MAX)
		send_error_and_exit(errnum, StringBuff<PATH_MAX + 20>{"execvp('", exec, "')"});
	else
		send_error_and_exit(errnum, StringBuff<PATH_MAX + 20>{"execvp('",
			exec.to_cstr().substring(0, PATH_MAX), "...')"});
}
