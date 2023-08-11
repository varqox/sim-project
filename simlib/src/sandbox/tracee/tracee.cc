#include "../communication/supervisor_pid1_tracee.hh"
#include "tracee.hh"

#include <ctime>
#include <fcntl.h>
#include <simlib/errmsg.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_path.hh>
#include <simlib/noexcept_concat.hh>
#include <simlib/string_view.hh>
#include <simlib/syscalls.hh>
#include <unistd.h>

namespace sandbox::tracee {

[[noreturn]] void main(Args args) noexcept {
    namespace sms = communication::supervisor_pid1_tracee;
    auto die_with_msg = [&] [[noreturn]] (auto&&... msg) noexcept {
        sms::write_result_error(
            args.shared_mem_state, "tracee: ", std::forward<decltype(msg)>(msg)...
        );
        _exit(1);
    };
    auto die_with_error = [&] [[noreturn]] (auto&&... msg) noexcept {
        die_with_msg(std::forward<decltype(msg)>(msg)..., errmsg());
    };
    auto exclude_pid1_from_tracee_session_and_process_group = [&]() noexcept {
        if (setsid() < 0) {
            die_with_error("setsid()");
        }
    };
    auto setup_std_fds = [&]() noexcept {
        if (args.stdin_fd && dup3(*args.stdin_fd, STDIN_FILENO, 0) < 0) {
            die_with_error("dup3()");
        }
        if (args.stdout_fd && dup3(*args.stdout_fd, STDOUT_FILENO, 0) < 0) {
            die_with_error("dup3()");
        }
        if (args.stderr_fd && dup3(*args.stderr_fd, STDERR_FILENO, 0) < 0) {
            die_with_error("dup3()");
        }
    };
    auto get_current_time = [&]() noexcept {
        timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts)) {
            die_with_error("clock_gettime()");
        }
        return ts;
    };

    exclude_pid1_from_tracee_session_and_process_group();
    setup_std_fds();

    if (args.argv.empty() || args.argv.back() != nullptr) {
        die_with_msg("BUG: argv array does not contain nullptr as the last element");
    }
    if (args.env.empty() || args.env.back() != nullptr) {
        die_with_msg("BUG: env array does not contain nullptr as the last element");
    }

    auto ts = get_current_time();
    sms::write(args.shared_mem_state->tracee_exec_start_time, ts);
    syscalls::execveat(args.executable_fd, "", args.argv.data(), args.env.data(), AT_EMPTY_PATH);
    die_with_error("execveat()");
}

} // namespace sandbox::tracee
