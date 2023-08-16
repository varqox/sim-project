#include "../communication/supervisor_pid1_tracee.hh"
#include "../tracee/tracee.hh"
#include "pid1.hh"

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <ctime>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <simlib/errmsg.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_path.hh>
#include <simlib/noexcept_concat.hh>
#include <simlib/overloaded.hh>
#include <simlib/string_view.hh>
#include <simlib/syscalls.hh>
#include <simlib/ubsan.hh>
#include <sys/capability.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <variant>

namespace sandbox::pid1 {

[[noreturn]] void main(Args args) noexcept {
    namespace sms = communication::supervisor_pid1_tracee;
    auto die_with_msg = [&] [[noreturn]] (auto&&... msg) noexcept {
        sms::write_result_error(
            args.shared_mem_state, "pid1: ", std::forward<decltype(msg)>(msg)...
        );
        _exit(1);
    };
    auto die_with_error = [&] [[noreturn]] (auto&&... msg) noexcept {
        die_with_msg(std::forward<decltype(msg)>(msg)..., errmsg());
    };
    auto set_process_name = [&]() noexcept {
        if (prctl(PR_SET_NAME, "pid1", 0, 0, 0)) {
            die_with_error("prctl(SET_NAME)");
        }
    };
    auto setup_kill_on_supervisor_death = [&](int supervisor_pidfd) noexcept {
        // Make kernel send us SIGKILL when the parent process (= supervisor process) dies
        if (prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0)) {
            die_with_error("prctl(PR_SET_PDEATHSIG)");
        }
        // Check if the supervisor is not already dead - it might happened just before prctl(). We
        // cannot use getppid() because it returns 0 as we are in a new PID namespace, so we use
        // poll() on supervisor's pidfd
        pollfd pfd = {
            .fd = supervisor_pidfd,
            .events = POLLIN,
            .revents = 0,
        };
        if (poll(&pfd, 1, 0) == 1) {
            die_with_msg("supervisor died");
        }
        // Close supervisor_pidfd to limit attack surface as it may be used to send signals to the
        // supervisor
        if (close(supervisor_pidfd)) {
            die_with_error("close()");
        }
    };
    auto setup_session_and_process_group = [&]() noexcept {
        // Exclude pid1 process from the parent's process group and session
        if (setsid() < 0) {
            die_with_error("setsid()");
        }
    };
    auto write_file = [&](FilePath file_path, StringView data) noexcept {
        auto fd = open(file_path, O_WRONLY | O_TRUNC | O_CLOEXEC);
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
    auto setup_user_namespace = [&](const Args::LinuxNamespaces::User::Pid1& user_ns) noexcept {
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
    auto setup_mount_namespace = [&](const Args::LinuxNamespaces::Mount& mount_ns) noexcept {
        if (chdir("/")) {
            die_with_error("chdir(\"/\")");
        }
        for (const auto& oper : mount_ns.operations) {
            using Mount = Args::LinuxNamespaces::Mount;
            std::visit(
                overloaded{
                    [&](const Mount::MountTmpfs& mount_tmpfs) {
                        auto flags = MS_NOSUID | MS_SILENT;
                        if (mount_tmpfs.read_only) {
                            flags |= MS_RDONLY;
                        }
                        if (mount_tmpfs.no_exec) {
                            flags |= MS_NOEXEC;
                        }
                        auto options_str = [&]() noexcept {
                            auto size = [&]() noexcept {
                                using T = decltype(mount_tmpfs.max_total_size_of_files_in_bytes
                                )::value_type;
                                if (!mount_tmpfs.max_total_size_of_files_in_bytes) {
                                    return T{0}; // no limit
                                }
                                auto x = *mount_tmpfs.max_total_size_of_files_in_bytes;
                                return x > 0 ? x : T{1}; // 1 == lowest possible limit
                            }();
                            auto nr_inodes = [&]() noexcept {
                                using T = decltype(mount_tmpfs.max_total_size_of_files_in_bytes
                                )::value_type;
                                if (!mount_tmpfs.inode_limit) {
                                    return T{0}; // no limit
                                }
                                // Adjust limit because root dir counts as an inode
                                return *mount_tmpfs.inode_limit +
                                    1; // overflow is fine, as 0 == no limit
                            }();
                            uint8_t mode_user = (mount_tmpfs.root_dir_mode >> 6) & 7;
                            uint8_t mode_group = (mount_tmpfs.root_dir_mode >> 3) & 7;
                            uint8_t mode_other = mount_tmpfs.root_dir_mode & 7;
                            return noexcept_concat(
                                "size=",
                                size,
                                ",nr_inodes=",
                                nr_inodes,
                                ",mode=0",
                                mode_user,
                                mode_group,
                                mode_other
                            );
                        }();
                        if (mount(
                                nullptr,
                                mount_tmpfs.path.c_str(),
                                "tmpfs",
                                flags,
                                options_str.c_str()
                            )) {
                            die_with_error("mount(tmpfs at \"", mount_tmpfs.path, "\")");
                        }
                    },
                    [&](const Mount::MountProc& mount_proc) {
                        auto flags = MS_NOSUID | MS_SILENT;
                        if (mount_proc.read_only) {
                            flags |= MS_RDONLY;
                        }
                        if (mount_proc.no_exec) {
                            flags |= MS_NOEXEC;
                        }
                        if (mount(nullptr, mount_proc.path.c_str(), "proc", flags, nullptr)) {
                            die_with_error("mount(proc at \"", mount_proc.path, "\")");
                        }
                    },
                    [&](const Mount::BindMount& bind_mount) {
                        int mount_fd = open_tree(
                            AT_FDCWD,
                            bind_mount.source.c_str(),
                            OPEN_TREE_CLOEXEC | OPEN_TREE_CLONE |
                                (bind_mount.recursive ? AT_RECURSIVE : 0)
                        );
                        if (mount_fd < 0) {
                            die_with_error("open_tree(\"", bind_mount.source, "\")");
                        }

                        mount_attr mattr = {};
                        mattr.attr_set = MOUNT_ATTR_NOSUID;
                        if (bind_mount.read_only) {
                            mattr.attr_set |= MOUNT_ATTR_RDONLY;
                        }
                        if (bind_mount.no_exec) {
                            mattr.attr_set |= MOUNT_ATTR_NOEXEC;
                        }
                        if (mount_setattr(
                                mount_fd,
                                "",
                                AT_EMPTY_PATH | (bind_mount.recursive ? AT_RECURSIVE : 0),
                                &mattr,
                                sizeof(mattr)
                            ))
                        {
                            die_with_error("mount_setattr()");
                        }
                        if (move_mount(
                                mount_fd,
                                "",
                                AT_FDCWD,
                                bind_mount.dest.c_str(),
                                MOVE_MOUNT_F_EMPTY_PATH
                            ))
                        {
                            die_with_error("move_mount(dest: \"", bind_mount.dest, "\")");
                        }
                        if (close(mount_fd)) {
                            die_with_error("close()");
                        }
                    },
                    [&](const Mount::CreateDir& create_dir) {
                        if (mkdir(create_dir.path.c_str(), create_dir.mode)) {
                            die_with_error("mkdir(\"", create_dir.path, "\")");
                        }
                    },
                    [&](const Mount::CreateFile& create_file) {
                        int fd = open(
                            create_file.path.c_str(), O_CREAT | O_EXCL | O_CLOEXEC, create_file.mode
                        );
                        if (fd < 0) {
                            die_with_error("open(\"", create_file.path, "\", O_CREAT | O_EXCL)");
                        }
                        if (close(fd)) {
                            die_with_error("close()");
                        }
                    },
                },
                oper
            );
        }

        if (mount_ns.new_root_mount_path) {
            if (chdir(mount_ns.new_root_mount_path->c_str())) {
                die_with_error("chdir(new_root_mount_path)");
            }
            // This has to be done within the same user namespace that performed the mount. After
            // the following clone3 with CLONE_NEWUSER | CLONE_NEWNS the whole mount tree becomes
            // locked in tracee and we really want it for security i.e. the user will not be able to
            // disintegrate part of the mount tree (but they may umount the entire mount tree).
            if (syscalls::pivot_root(".", ".")) {
                die_with_error(R"(pivot_root(".", "."))");
            }
            // Unmount the old root (also, it is needed for clone3 with CLONE_NEWUSER to succeed)
            if (umount2(".", MNT_DETACH)) {
                die_with_error(R"(umount2("."))");
            }
        }
    };
    auto drop_all_capabilities_and_prevent_gaining_any_of_them = [&]() noexcept {
        cap_t caps = cap_init(); // all capabilities are cleared
        if (caps == nullptr) {
            die_with_error("caps_init()");
        }
        if (cap_set_proc(caps)) {
            die_with_error("cap_set_proc()");
        }
        if (cap_free(caps)) {
            die_with_error("cap_free()");
        }
    };
    auto harden_against_potential_compromise = [&]() noexcept {
        // Cut access to cgroups other than ours
        unshare(CLONE_NEWCGROUP);

        // TODO: install seccomp filters
    };
    auto get_current_time = [&]() noexcept {
        timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts)) {
            die_with_error("clock_gettime()");
        }
        return ts;
    };

    set_process_name();
    setup_kill_on_supervisor_death(args.supervisor_pidfd);
    setup_session_and_process_group();
    setup_user_namespace(args.linux_namespaces.user.pid1);
    int proc_dirfd = open("/proc", O_RDONLY | O_CLOEXEC);
    setup_mount_namespace(args.linux_namespaces.mount);
    drop_all_capabilities_and_prevent_gaining_any_of_them();

    if (UNDEFINED_SANITIZER) {
        auto ignore_signal = [&](int sig) noexcept {
            struct sigaction sa = {};
            sa.sa_handler = SIG_IGN;
            if (sigemptyset(&sa.sa_mask)) {
                die_with_error("sigemptyset()");
            }
            sa.sa_flags = 0;
            if (sigaction(sig, &sa, nullptr)) {
                die_with_error("sigaction()");
            }
        };
        // Undefined sanitizer installs signal handlers for these signals and this leaves pid1
        // process prone to being killed by these signals
        ignore_signal(SIGBUS);
        ignore_signal(SIGFPE);
        ignore_signal(SIGSEGV);
    }

    clone_args cl_args = {};
    // CLONE_NEWUSER | CLONE_NEWNS are needed to lock the mount tree
    cl_args.flags = CLONE_NEWCGROUP | CLONE_INTO_CGROUP | CLONE_NEWUSER | CLONE_NEWNS;
    cl_args.exit_signal = SIGCHLD;
    cl_args.cgroup = static_cast<uint64_t>(args.tracee_cgroup_fd);

    auto tracee_pid = syscalls::clone3(&cl_args);
    if (tracee_pid == -1) {
        die_with_error("clone3()");
    }
    if (tracee_pid == 0) {
        tracee::main({
            .shared_mem_state = args.shared_mem_state,
            .executable_fd = args.executable_fd,
            .stdin_fd = args.stdin_fd,
            .stdout_fd = args.stdout_fd,
            .stderr_fd = args.stderr_fd,
            .argv = std::move(args.argv),
            .env = std::move(args.env),
            .proc_dirfd = proc_dirfd,
            .tracee_cgroup_cpu_stat_fd = args.tracee_cgroup_cpu_stat_fd,
            .linux_namespaces =
                {
                    .user =
                        {
                            .outside_uid = args.linux_namespaces.user.tracee.outside_uid,
                            .inside_uid = args.linux_namespaces.user.tracee.inside_uid,
                            .outside_gid = args.linux_namespaces.user.tracee.outside_gid,
                            .inside_gid = args.linux_namespaces.user.tracee.inside_gid,
                        },
                },
            .prlimit = args.prlimit,
        });
    }

    // Close all file descriptors
    if (close_range(4, ~0U, 0)) {
        die_with_error("close_range()");
    }

    harden_against_potential_compromise();

    timespec waitid_time;
    siginfo_t si;
    for (;;) {
        if (syscalls::waitid(P_ALL, 0, &si, __WALL | WEXITED, nullptr)) {
            if (errno == ECHILD) {
                break;
            }
            die_with_error("waitid()");
        }
        waitid_time = get_current_time();
        if (si.si_pid == tracee_pid) {
            // Remaining processes will be killed on pid1's death
            break;
        }
    }

    // Check if tracee died prematurely with an error
    if (sms::is<sms::result::Error>(args.shared_mem_state)) {
        // Propagate error
        _exit(1); // error is already written by tracee
    }

    sms::write(args.shared_mem_state->tracee_waitid_time, waitid_time);
    sms::write_result_ok(
        args.shared_mem_state,
        {
            .code = si.si_code,
            .status = si.si_status,
        }
    );

    _exit(0);
}

} // namespace sandbox::pid1
