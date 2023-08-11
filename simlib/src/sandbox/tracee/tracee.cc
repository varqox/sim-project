#include "../communication/supervisor_tracee.hh"
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
    namespace sms = communication::supervisor_tracee;
    auto die_with_msg = [&] [[noreturn]] (auto&&... msg) noexcept {
        sms::write_error(args.supervisor_shared_mem_state, std::forward<decltype(msg)>(msg)...);
        _exit(1);
    };
    auto die_with_error = [&] [[noreturn]] (auto&&... msg) noexcept {
        die_with_msg(std::forward<decltype(msg)>(msg)..., errmsg());
    };
    auto write_file = [&](FilePath file_path, StringView data) noexcept {
        int fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC);
        if (fd == -1) {
            die_with_error("open(", file_path, ")");
        }
        if (write_all(fd, data) != data.size()) {
            die_with_error("write(", file_path, ")");
        }
        if (close(fd)) {
            die_with_error("close()");
        }
    };
    auto setup_user_namespace = [&](const Args::LinuxNamespaces::User& user_ns) noexcept {
        write_file(
            "/proc/self/uid_map",
            from_unsafe{noexcept_concat(user_ns.inside_uid, ' ', user_ns.outside_uid, " 1")}
        );
        write_file("/proc/self/setgroups", "deny");
        write_file(
            "/proc/self/gid_map",
            from_unsafe{noexcept_concat(user_ns.inside_gid, ' ', user_ns.outside_gid, " 1")}
        );
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

    setup_user_namespace(args.linux_namespaces.user);
    setup_std_fds();

    if (args.argv.empty() || args.argv.back() != nullptr) {
        die_with_msg("BUG: argv array does not contain nullptr as the last element");
    }
    if (args.env.empty() || args.env.back() != nullptr) {
        die_with_msg("BUG: env array does not contain nullptr as the last element");
    }

    auto ts = get_current_time();
    sms::write(args.supervisor_shared_mem_state->tracee_exec_start_time, ts);
    syscalls::execveat(args.executable_fd, "", args.argv.data(), args.env.data(), AT_EMPTY_PATH);
    die_with_error("execveat()");
}

} // namespace sandbox::tracee
