#include "../communication/supervisor_tracee.hh"
#include "tracee.hh"

#include <fcntl.h>
#include <simlib/errmsg.hh>
#include <simlib/syscalls.hh>
#include <unistd.h>

namespace sandbox::tracee {

[[noreturn]] void main(Args args) noexcept {
    auto die_with_msg = [&] [[noreturn]] (auto&&... msg) noexcept {
        communication::supervisor_tracee::write_error(
            args.supervisor_shared_mem_state, std::forward<decltype(msg)>(msg)...
        );
        _exit(1);
    };
    auto die_with_error = [&] [[noreturn]] (auto&&... msg) noexcept {
        die_with_msg(std::forward<decltype(msg)>(msg)..., errmsg());
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

    setup_std_fds();

    if (args.argv.empty() || args.argv.back() != nullptr) {
        die_with_msg("BUG: argv array does not contain nullptr as the last element");
    }
    if (args.env.empty() || args.env.back() != nullptr) {
        die_with_msg("BUG: env array does not contain nullptr as the last element");
    }

    syscalls::execveat(args.executable_fd, "", args.argv.data(), args.env.data(), AT_EMPTY_PATH);
    die_with_error("execveat()");
}

} // namespace sandbox::tracee
