#pragma once

#include "filesystem.h"
#include "time.h"

#include <functional>
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
	 *     signals, it must install them again after the function returns in
	 *     all threads.
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
		writeAll(fd, err.data(), err.size);

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
	static void run_child(FilePath exec,
		const std::vector<std::string>& exec_args, const Options& opts, int fd,
		std::function<void()> doBeforeExec) noexcept;

	static void defaultTimeoutHandler(pid_t pid) { kill(-pid, SIGKILL); };

	class Timer {
	public:
		using TimeoutHandler = std::function<void(pid_t)>;

	private:
		struct Data {
			pid_t pid;
			TimeoutHandler timeouter;
		} data;
		timespec tlimit;
		// used only if time_limit == {0, 0}
		timespec begin_point;
		// used only if time_limit != {0, 0}
		timer_t timerid;
		bool timer_is_active = false;

		static void handle_timeout(int, siginfo_t* si, void*) noexcept;

		void delete_timer() noexcept;

	public:
		Timer(pid_t pid, timespec time_limit,
			TimeoutHandler timeouter = defaultTimeoutHandler);

		timespec stop_and_get_runtime();

		~Timer() { delete_timer(); }
	};

	class CPUTimeMonitor {
	public:
		using TimeoutHandler = std::function<void(pid_t)>;

	private:
		struct Data {
			pid_t pid;
			clockid_t cid;
			timespec cpu_abs_time_limit; // Counting from 0, not from cpu_time_at_start
			timespec cpu_time_at_start;
			timer_t timerid;
			TimeoutHandler timeouter;
		} data;
		bool timer_is_active = false;

		static void handler(int, siginfo_t* si, void*) noexcept;

	public:
		CPUTimeMonitor(pid_t pid, timespec cpu_time_limit,
			TimeoutHandler timeouter = defaultTimeoutHandler);

		void deactivate() noexcept;

		timespec get_cpu_runtime();

		~CPUTimeMonitor() { deactivate(); }
	};
};
