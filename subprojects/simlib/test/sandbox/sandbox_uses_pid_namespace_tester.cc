#include <cerrno>
#include <cstddef>
#include <fcntl.h>
#include <regex>
#include <simlib/file_descriptor.hh>
#include <simlib/noexcept_concat.hh>
#include <simlib/read_file.hh>
#include <simlib/string_transform.hh>
#include <simlib/syscalls.hh>
#include <simlib/throw_assert.hh>
#include <simlib/utilities.hh>
#include <unistd.h>

void check_pids() {
    throw_assert(getpid() == 2);
    throw_assert(getppid() == 1);
}

void check_killing_with_all_signals() {
    // With no signal
    throw_assert(kill(-1, 0) == -1 && errno == ESRCH); // kill everyone we can except ourselves
    throw_assert(kill(1, 0) == 0); // try to kill init
    throw_assert(kill(0, 0) == 0); // kill our process group
    throw_assert(kill(getpid(), 0) == 0); // kill ourselves

    for (int sig = 1; sig <= SIGRTMAX; ++sig) {
        // Ignore signals used by libc
        if (__SIGRTMIN <= sig && sig < SIGRTMIN) {
            continue;
        }

        sigset_t ss;
        throw_assert(sigemptyset(&ss) == 0);
        throw_assert(sigaddset(&ss, sig) == 0);
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        throw_assert(sigprocmask(SIG_BLOCK, &ss, nullptr) == 0);

        auto pending_sigs_num = [&]() -> size_t {
            for (size_t i = 0;; ++i) {
                timespec ts = {.tv_sec = 0, .tv_nsec = 0};
                auto rc = sigtimedwait(&ss, nullptr, &ts);
                if (rc == sig) {
                    continue;
                }
                throw_assert(rc == -1 && errno == EAGAIN);
                return i;
            }
        };

        throw_assert(pending_sigs_num() == 0);
        throw_assert(
            kill(-1, sig) == -1 && errno == ESRCH
        ); // kill everyone we can except ourselves
        throw_assert(pending_sigs_num() == 0);
        throw_assert(kill(1, sig) == 0); // try to kill init
        throw_assert(pending_sigs_num() == 0);
        if (!is_one_of(sig, SIGKILL, SIGSTOP)) {
            throw_assert(kill(0, sig) == 0); // kill our process group
            throw_assert(pending_sigs_num() == 1);
            throw_assert(kill(getpid(), sig) == 0); // kill ourselves
            throw_assert(pending_sigs_num() == 1);
        }
    }
}

void check_killing_supervisor_using_pidfd_from_opening_proc_subdir() {
    pid_t procfs_pid = [] {
        auto proc_self_status_contents = read_file("/proc/self/status");
        std::smatch parts;
        throw_assert(
            std::regex_search(proc_self_status_contents, parts, std::regex{"\nPid:\\s*(\\d*)"})
        );
        return str2num<pid_t>(from_unsafe{parts[1].str()}).value();
    }();

    throw_assert(procfs_pid != 0);
    throw_assert(procfs_pid != 2); // procfs should be from the parent pid namespace

    pid_t procfs_parent_pid = [] {
        auto proc_self_status_contents = read_file("/proc/self/status");
        std::smatch parts;
        throw_assert(
            std::regex_search(proc_self_status_contents, parts, std::regex{"\nPPid:\\s*(\\d*)"})
        );
        return str2num<pid_t>(from_unsafe{parts[1].str()}).value();
    }();
    throw_assert(procfs_parent_pid != 0);
    throw_assert(procfs_parent_pid != 1); // procfs should be from the parent pid namespace

    pid_t procfs_grandparent_pid = [procfs_parent_pid] {
        auto proc_parent_status_contents =
            read_file(noexcept_concat("/proc/", procfs_parent_pid, "/status"));
        std::smatch parts;
        throw_assert(
            std::regex_search(proc_parent_status_contents, parts, std::regex{"\nPPid:\\s*(\\d*)"})
        );
        return str2num<pid_t>(from_unsafe{parts[1].str()}).value();
    }();
    throw_assert(procfs_grandparent_pid != 0);

    auto grandparent_pidfd = FileDescriptor{
        open(noexcept_concat("/proc/", procfs_grandparent_pid).c_str(), O_RDONLY | O_CLOEXEC)
    };
    // Send signal to the grandparent (aka supervisor)
    throw_assert(
        syscalls::pidfd_send_signal(grandparent_pidfd, SIGKILL, nullptr, 0) == -1 && errno == EINVAL
    );
}

// Tests pids and kill isolation
int main() {
    check_pids();
    check_killing_with_all_signals();
    check_killing_supervisor_using_pidfd_from_opening_proc_subdir();
}
