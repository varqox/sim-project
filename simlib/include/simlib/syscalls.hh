#pragma once

#include <csignal>
#include <linux/sched.h>
#include <linux/wait.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

extern "C" struct rusage;

namespace syscalls {

#ifdef SYS_gettid
inline pid_t gettid() noexcept { return static_cast<pid_t>(syscall(SYS_gettid)); }
#endif

#ifdef SYS_waitid
inline int
waitid(int id_type, pid_t id, siginfo_t* info, int options, struct rusage* usage) noexcept {
    return static_cast<int>(syscall(SYS_waitid, id_type, id, info, options, usage));
}
#endif

#ifdef SYS_pidfd_open
inline int pidfd_open(pid_t pid, unsigned int flags) noexcept {
    return static_cast<int>(syscall(SYS_pidfd_open, pid, flags));
}
#endif

#ifdef SYS_pidfd_getfd
inline int pidfd_getfd(int pidfd, int targetfd, unsigned int flags) noexcept {
    return static_cast<int>(syscall(SYS_pidfd_getfd, pidfd, targetfd, flags));
}
#endif

#ifdef SYS_pidfd_send_signal
inline int
pidfd_send_signal(int pidfd, int sig, siginfo_t* info, unsigned int flags) noexcept {
    return static_cast<int>(syscall(SYS_pidfd_send_signal, pidfd, sig, info, flags));
}
#endif

#ifdef SYS_pivot_root
inline int pivot_root(const char* new_root, const char* put_old) noexcept {
    return static_cast<int>(syscall(SYS_pivot_root, new_root, put_old));
}
#endif

#ifdef SYS_clone3
// NOLINTNEXTLINE(google-runtime-int)
inline long clone3(struct clone_args* cl_args, size_t size) noexcept {
    return syscall(SYS_clone3, cl_args, size);
}
#endif

#ifdef SYS_clone3
// NOLINTNEXTLINE(google-runtime-int)
inline long clone3(struct clone_args* cl_args) noexcept {
    return syscall(SYS_clone3, cl_args, sizeof(*cl_args));
}
#endif

#ifdef SYS_execveat
inline int execveat(
    int dirfd, const char* pathname, char* const argv[], char* const envp[],
    int flags) noexcept {
    return static_cast<int>(syscall(SYS_execveat, dirfd, pathname, argv, envp, flags));
}
#endif

} // namespace syscalls
