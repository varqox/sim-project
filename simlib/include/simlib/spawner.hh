#pragma once

#include <atomic>
#include <chrono>
#include <csignal>
#include <ctime>
#include <functional>
#include <optional>
#include <pthread.h>
#include <simlib/file_contents.hh>
#include <simlib/file_path.hh>
#include <simlib/overloaded.hh>
#include <sys/resource.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <variant>

class Spawner {
protected:
    Spawner() = default;

public:
    struct ExitStat {
        std::chrono::nanoseconds runtime{0};
        std::chrono::nanoseconds cpu_runtime{0};

        struct {
            int code; // si_code field from siginfo_t from waitid(2)
            int status; // si_status field from siginfo_t from waitid(2)
        } si{};
        struct rusage rusage = {}; // resource information
        uint64_t vm_peak = 0; // peak virtual memory size (in bytes)
        std::string message;

        ExitStat() = default;

        ExitStat(
            std::chrono::nanoseconds rt,
            std::chrono::nanoseconds cpu_time,
            int sic,
            int sis,
            const struct rusage& rus,
            uint64_t vp,
            std::string msg = {}
        )
        : runtime(rt)
        , cpu_runtime(cpu_time)
        , si{sic, sis}
        , rusage(rus)
        , vm_peak(vp)
        , message(std::move(msg)) {}

        ExitStat(const ExitStat&) = default;
        ExitStat(ExitStat&&) noexcept = default;
        ExitStat& operator=(const ExitStat&) = default;
        ExitStat& operator=(ExitStat&&) = default;

        ~ExitStat() = default;
    };

    struct Options {
        int new_stdin_fd; // negative - close, STDIN_FILENO - do not change
        int new_stdout_fd; // negative - close, STDOUT_FILENO - do not change
        int new_stderr_fd; // negative - close, STDERR_FILENO - do not change
        std::optional<std::chrono::nanoseconds> real_time_limit;
        std::optional<uint64_t> memory_limit; // in bytes
        std::optional<std::chrono::nanoseconds>
            cpu_time_limit; // if not set and real time limit is set, then CPU
                            // time limit will be set to round(real time limit
                            // in seconds) + 1 seconds
        CStringView working_dir; // directory at which program will be run

        constexpr Options() : Options(STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO) {}

        constexpr Options(int ifd, int ofd, int efd, CStringView wd)
        : new_stdin_fd(ifd)
        , new_stdout_fd(ofd)
        , new_stderr_fd(efd)
        , working_dir(wd) {}

        constexpr Options(
            int ifd,
            int ofd,
            int efd,
            std::optional<std::chrono::nanoseconds> rtl = std::nullopt,
            std::optional<uint64_t> ml = std::nullopt,
            std::optional<std::chrono::nanoseconds> ctl = std::nullopt,
            CStringView wd = CStringView(".")
        )
        : new_stdin_fd(ifd)
        , new_stdout_fd(ofd)
        , new_stderr_fd(efd)
        , real_time_limit(rtl)
        , memory_limit(ml)
        , cpu_time_limit(ctl)
        , working_dir(wd) {}
    };

    /**
     * @brief Runs @p exec with arguments @p exec_args and limits:
     *   @p opts.time_limit and @p opts.memory_limit
     * @details @p exec is called via execvp()
     *   This function is thread-safe.
     *   IMPORTANT: To function properly this function uses internally signal
     *     SIGRTMIN and installs handler for it. So be aware that using these
     *     signals while this function runs (in any thread) is not safe.
     *     Moreover if your program installed handler for the above signals, it
     *     must install them again after the function returns in all threads.
     *
     *
     * @param exec path to file will be executed
     * @param exec_args arguments passed to exec
     * @param opts options (new_stdin_fd, new_stdout_fd, new_stderr_fd - file
     *   descriptors to which respectively stdin, stdout, stderr of spawned
     *   process will be changed or if negative, closed;
     *   time_limit set to std::nullopt disables the time limit;
     *   cpu_time_limit set to std::nullopt disables the CPU time limit;
     *   memory_limit set to std::nullopt disables memory limit;
     *   working_dir set to "", "." or "./" disables changing working
     *   directory)
     * @param do_in_parent_after_fork function taking child's pid as an argument
     *   that will be called in the parent process just after fork() -- useful
     *   for closing pipe ends
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
    static ExitStat run(
        FilePath exec,
        const std::vector<std::string>& exec_args,
        const Options& opts = Options(),
        const std::function<void(pid_t)>& do_in_parent_after_fork = [](pid_t /*unused*/) {}
    );

protected:
    // Sends @p str through @p fd and _exits with -1
    static void send_error_message_and_exit(int fd, CStringView str) noexcept {
        (void)write_all(fd, str.data(), str.size());
        _exit(-1);
    };

    // Sends @p str followed by error message of @p errnum through @p fd and
    // _exits with -1
    static void send_error_message_and_exit(int fd, int errnum, CStringView str) noexcept {
        (void)write_all(fd, str.data(), str.size());

        auto err = errmsg(errnum);
        (void)write_all(fd, err.data(), err.size());

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
     *   time_limit set to std::nullopt disables time limit;
     *   cpu_time_limit set to std::nullopt disables CPU time limit;
     *   memory_limit set to std::nullopt disables memory limit;
     *   working_dir set to "", "." or "./" disables changing working
     *   directory)
     * @param fd file descriptor to which errors will be written
     * @param do_before_exec function that is to be called before executing
     *   @p exec
     */
    static void run_child(
        FilePath exec,
        const std::vector<std::string>& exec_args,
        const Options& opts,
        int fd,
        const std::function<void()>& do_before_exec
    ) noexcept;

    class Timer {
        struct SignalHandlerContext {
            const pid_t watched_pid;
            const int timeout_signal;
            volatile std::sig_atomic_t timeout_signal_was_sent;
        };

        struct WithTimeout {
            const timespec time_limit;
            timer_t timer_id;
            bool timer_is_active;
            SignalHandlerContext signal_handler_context;
        };

        struct WithoutTimeout {
            const timespec start_clock_time;
        };

        const clockid_t clock_id_;
        const pid_t creator_thread_id_;
        std::variant<WithTimeout, WithoutTimeout> state_;

        timespec delete_timer_and_get_remaning_time() noexcept;

    public:
        /**
         * @param watched_pid pid of the process to signal on timeout
         * @param time_limit if set to 0, then timeout handler is not installed,
         *thus @p timer_signal and @p timeout_signal are ignored
         * @param clock_id id of the clock to crate timer on
         * @param timeout_signal signal to send @p to watched_pid on timeout
         * @param timer_signal signal for which a timeout handler will be
         *installed
         **/
        Timer(
            pid_t watched_pid,
            std::chrono::nanoseconds time_limit,
            clockid_t clock_id,
            int timeout_signal = SIGKILL,
            int timer_signal = SIGRTMIN
        );

        Timer(const Timer&) = delete;
        Timer(Timer&&) = delete;
        Timer& operator=(const Timer&) = delete;
        Timer& operator=(Timer&&) = delete;

        std::chrono::nanoseconds deactivate_and_get_runtime() noexcept;

        [[nodiscard]] bool timeout_signal_was_sent() const noexcept;

        ~Timer() { (void)delete_timer_and_get_remaning_time(); }
    };
};
