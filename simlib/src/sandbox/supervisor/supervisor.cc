#include "../communication/client_supervisor.hh"
#include "../communication/supervisor_pid1_tracee.hh"
#include "../do_die_with_error.hh"
#include "../pid1/pid1.hh"
#include "cgroups/read_cpu_times.hh"
#include "request/deserialize.hh"
#include "request/request.hh"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <linux/prctl.h>
#include <linux/securebits.h>
#include <memory>
#include <optional>
#include <poll.h>
#include <sched.h>
#include <simlib/array_vec.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/deserialize.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_path.hh>
#include <simlib/file_perms.hh>
#include <simlib/from_unsafe.hh>
#include <simlib/macros/wont_throw.hh>
#include <simlib/meta/min.hh>
#include <simlib/noexcept_concat.hh>
#include <simlib/overloaded.hh>
#include <simlib/sandbox/si.hh>
#include <simlib/socket_stream_ext.hh>
#include <simlib/static_cstring_buff.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_transform.hh>
#include <simlib/string_view.hh>
#include <simlib/syscalls.hh>
#include <simlib/timespec_arithmetic.hh>
#include <simlib/utilities.hh>
#include <string_view>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <variant>
#include <vector>

using std::optional;

namespace sandbox::supervisor {

void check_that_we_do_not_run_as_root() noexcept {
    struct stat st;
    if (stat("/dev/null", &st)) {
        do_die_with_error(STDERR_FILENO, "stat()");
    }
    bool dev_null_is_dev_null = S_ISCHR(st.st_mode) && st.st_rdev == makedev(1, 3);
    auto root_real_uid = st.st_uid;
    auto root_real_gid = st.st_gid;
    if (!dev_null_is_dev_null || is_one_of(geteuid(), 0U, root_real_uid) ||
        is_one_of(getegid(), 0U, root_real_gid))
    {
        do_die_with_msg(STDERR_FILENO, "supervisor is not safe to be run by root");
    }
}

struct CmdArgs {
    int sock_fd;
};

[[nodiscard]] CmdArgs parse_args(int argc, char** argv) noexcept {
    if (argc != 2) {
        do_die_with_msg(
            STDERR_FILENO,
            "Usage: ",
            (argv[0] == nullptr || argv[0][0] == '\0' ? "sandbox-supervisor" : argv[0]),
            " <unix socket file descriptor number>"
        );
    }

    auto sock_fd_opt = str2num<int>(argv[1]);
    if (not sock_fd_opt) {
        do_die_with_msg(STDERR_FILENO, "supervisor: invalid file descriptor number as argument");
    }

    return {
        .sock_fd = *sock_fd_opt,
    };
}

void validate_sock_fd(int sock_fd) noexcept {
    auto get_sock_opt = [sock_fd](auto sock_opt) noexcept {
        int optval;
        socklen_t optlen = sizeof(optval);
        if (getsockopt(sock_fd, SOL_SOCKET, sock_opt, &optval, &optlen)) {
            do_die_with_msg(
                STDERR_FILENO, "supervisor: invalid file descriptor (getsockopt()", errmsg(), ")"
            );
        }
        assert(optlen == sizeof(optval));
        return optval;
    };

    if (get_sock_opt(SO_DOMAIN) != AF_UNIX) {
        do_die_with_msg(STDERR_FILENO, "supervisor: invalid socket domain, expected AF_UNIX");
    }

    // We want unlimited message size and send()/recv() errors if the second end is closed
    if (get_sock_opt(SO_TYPE) != SOCK_STREAM) {
        do_die_with_msg(STDERR_FILENO, "supervisor: invalid socket type, expected SOCK_STREAM");
    }
}

constexpr int ERRORS_FD = STDERR_FILENO + 1;
constexpr int SOCK_FD = STDERR_FILENO + 2;

void initialize_sock_fd_and_error_fd(int sock_fd) noexcept {
    // There can be trouble if sock_fd is one of {STDIN,STDOUT,STDERR}_FILENO because we will reopen
    // them, so we make sure to use a different fd
    static_assert(
        SOCK_FD != STDERR_FILENO, "we cannot lose STDERR_FILENO in the following operations"
    );
    if (sock_fd == SOCK_FD) {
        // Set CLOEXEC flag so that we don't leak the file descriptor on exec()
        if (fcntl(SOCK_FD, F_SETFD, FD_CLOEXEC)) {
            do_die_with_error(STDERR_FILENO, "supervisor: fcntl()");
        }
    } else {
        if (dup3(sock_fd, SOCK_FD, O_CLOEXEC) == -1) {
            do_die_with_error(STDERR_FILENO, "supervisor: dup3()");
        }
    }

    static_assert(ERRORS_FD != SOCK_FD, "we cannot lose SOCK_FD in the following operations");
    static_assert(STDERR_FILENO != ERRORS_FD, "stderr will be reopen as /dev/null");
    if (dup3(STDERR_FILENO, ERRORS_FD, O_CLOEXEC) == -1) {
        do_die_with_error(STDERR_FILENO, "supervisor: dup3()");
    }
}

template <class... Args>
[[noreturn]] void die_with_msg(Args&&... msg) noexcept {
    sandbox::do_die_with_msg(ERRORS_FD, "supervisor: ", std::forward<Args>(msg)...);
}

template <class... Args>
[[noreturn]] void die_with_error(Args&&... msg) noexcept {
    sandbox::do_die_with_error(ERRORS_FD, "supervisor: ", std::forward<Args>(msg)...);
}

void set_process_name() noexcept {
    if (prctl(PR_SET_NAME, "supervisor", 0, 0, 0)) {
        die_with_error("prctl(SET_NAME)");
    }
}

void close_all_stray_file_descriptors() noexcept {
    static_assert(STDIN_FILENO == 0);
    static_assert(STDOUT_FILENO == 1);
    static_assert(STDERR_FILENO == 2);
    static_assert(ERRORS_FD == 3);
    static_assert(SOCK_FD == 4);
    if (close_range(SOCK_FD + 1, ~0U, 0)) {
        die_with_error("close_range()");
    }
}

void replace_stdin_stdout_stderr_with_dev_null() noexcept {
    int fd = open("/dev/null", O_RDONLY); // NOLINT(android-cloexec-open)
    if (fd == -1) {
        die_with_error("open(/dev/null)");
    }
    if (dup2(fd, STDIN_FILENO) == -1) {
        die_with_error("dup2()");
    }
    if (close(fd)) {
        die_with_error("close()");
    }
    fd = open("/dev/null", O_WRONLY); // NOLINT(android-cloexec-open)
    if (fd == -1) {
        die_with_error("open(/dev/null)");
    }
    if (dup2(fd, STDOUT_FILENO) == -1 || dup2(fd, STDERR_FILENO) == -1) {
        die_with_error("dup2()");
    }
    if (close(fd)) {
        die_with_error("close()");
    }
}

#ifndef SCM_MAX_FD
#define SCM_MAX_FD 253
#endif

ArrayVec<int, SCM_MAX_FD>
recv_request_header_with_fds(std::byte* buff, size_t header_len) noexcept {
    struct iovec iov = {
        .iov_base = buff,
        .iov_len = header_len,
    };

    alignas(struct cmsghdr) char ancillary_data_buff[CMSG_SPACE(SCM_MAX_FD * sizeof(int))];
    struct msghdr msg = {
        .msg_name = nullptr,
        .msg_namelen = 0,
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = ancillary_data_buff,
        .msg_controllen = sizeof(ancillary_data_buff),
        .msg_flags = 0, // it is ignored, but set it anyway
    };

    ssize_t rc;
    do {
        rc = recvmsg(SOCK_FD, &msg, MSG_CMSG_CLOEXEC);
    } while (rc == -1 && errno == EINTR);

    if (rc < 0) {
        die_with_error("recvmsg()");
    }
    if (rc == 0) {
        _exit(0); // No more requests
    }
    if (static_cast<size_t>(rc) != header_len) {
        die_with_msg(
            "recv_request_header_with_fds(): received ",
            rc,
            " bytes but header has ",
            header_len,
            " bytes"
        );
    }
    if (msg.msg_flags != MSG_CMSG_CLOEXEC) {
        die_with_msg("recv_request_header_with_fds(): unexpected msg_flags = ", msg.msg_flags);
    }

    ArrayVec<int, SCM_MAX_FD> fds;
    for (auto cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level != SOL_SOCKET) {
            die_with_msg("recv_request_header_with_fds(): unexpected cmsg_level");
        }
        if (cmsg->cmsg_type != SCM_RIGHTS) {
            die_with_msg("recv_request_header_with_fds(): unexpected cmsg_type");
        }
        assert(cmsg->cmsg_len >= CMSG_LEN(0));
        auto fds_num = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        assert(cmsg->cmsg_len == CMSG_LEN(fds_num * sizeof(int)));
        if (fds_num > SCM_MAX_FD - fds.size()) {
            die_with_msg("recv_request_header_with_fds(): received too many file descriptors");
        }
        for (size_t i = 0; i < fds_num; ++i) {
            int fd;
            std::memcpy(
                &fd, reinterpret_cast<std::byte*>(CMSG_DATA(cmsg)) + i * sizeof(int), sizeof(int)
            );
            fds.push(fd);
        }
    }
    return fds;
}

request::Request recv_request() noexcept {
    // Receive header
    communication::client_supervisor::request::body_len_t body_len;
    auto fds =
        recv_request_header_with_fds(reinterpret_cast<std::byte*>(&body_len), sizeof(body_len));

    request::Request req;
    try {
        req.buff = std::make_unique<std::byte[]>(body_len);
    } catch (...) {
        die_with_msg("recv_request(): failed to allocate memory (", body_len, " bytes)");
    }

    // Receive body
    if (recv_exact(SOCK_FD, req.buff.get(), body_len, 0)) {
        die_with_error("recv()");
    }
    try {
        auto reader = deserialize::Reader{req.buff.get(), body_len};
        request::deserialize(reader, fds, req);
        auto remaining_size = reader.remaining_size();
        if (remaining_size != 0) {
            die_with_msg(
                "recv_request(): invalid body len: unnecessary ", remaining_size, " bytes"
            );
        }
    } catch (const std::exception& e) {
        die_with_msg("recv_request(): deserialization error: ", e.what());
    }
    return req;
}

void close_request_fds(const request::Request& req) noexcept {
    if (close(req.executable_fd)) {
        die_with_error("close()");
    }
    if (req.stdin_fd && close(*req.stdin_fd)) {
        die_with_error("close()");
    }
    if (req.stdout_fd && close(*req.stdout_fd)) {
        die_with_error("close()");
    }
    if (req.stderr_fd && close(*req.stderr_fd)) {
        die_with_error("close()");
    }
}

namespace response {

struct Ok {
    Si tracee_si;
    timespec tracee_runtime;

    struct TraceeCgroup {
        struct CpuTime {
            uint64_t user_usec;
            uint64_t system_usec;
        } cpu_time;

        uint64_t peak_memory_in_bytes;
    } tracee_cgroup;
};

struct Error {
    std::string_view description;
};

void send_ok(Ok resp) noexcept {
    namespace response = communication::client_supervisor::response;
    if (send_as_bytes(
            SOCK_FD,
            MSG_NOSIGNAL,
            static_cast<response::error_len_t>(0),
            static_cast<response::si::code_t>(resp.tracee_si.code),
            static_cast<response::si::status_t>(resp.tracee_si.status),
            static_cast<response::time::sec_t>(resp.tracee_runtime.tv_sec),
            static_cast<response::time::nsec_t>(resp.tracee_runtime.tv_nsec),
            static_cast<response::cgroup::usec_t>(resp.tracee_cgroup.cpu_time.user_usec),
            static_cast<response::cgroup::usec_t>(resp.tracee_cgroup.cpu_time.system_usec),
            static_cast<response::cgroup::peak_memory_in_bytes_t>(
                resp.tracee_cgroup.peak_memory_in_bytes
            )
        ))
    {
        die_with_error("send()");
    }
}

void send_error(Error resp) noexcept {
    namespace response = communication::client_supervisor::response;
    if (send_as_bytes(
            SOCK_FD, MSG_NOSIGNAL, static_cast<response::error_len_t>(resp.description.size())
        ))
    {
        die_with_error("send()");
    }
    if (send_exact(SOCK_FD, resp.description.data(), resp.description.size(), MSG_NOSIGNAL)) {
        die_with_error("send()");
    }
}

} // namespace response

void read_shared_mem_state_and_send_response(
    volatile communication::supervisor_pid1_tracee::SharedMemState* shared_mem_state,
    Si pid1_si,
    const cgroups::CpuTimes& tracee_cpu_times,
    uint64_t tracee_cgroup_peak_memory_in_bytes
) noexcept {
    namespace sms = communication::supervisor_pid1_tracee;
    auto res = sms::read_result(shared_mem_state);
    std::visit(
        overloaded{
            [&](const sms::result::Ok& res_ok) noexcept {
                auto tracee_exec_start_time_opt =
                    sms::read(shared_mem_state->tracee_exec_start_time);
                auto tracee_exec_start_cpu_time_user_usec_opt =
                    sms::read(shared_mem_state->tracee_exec_start_cpu_time_user);
                auto tracee_exec_start_cpu_time_system_usec_opt =
                    sms::read(shared_mem_state->tracee_exec_start_cpu_time_system);
                if (!tracee_exec_start_time_opt || !tracee_exec_start_cpu_time_user_usec_opt ||
                    !tracee_exec_start_cpu_time_system_usec_opt)
                {
                    try {
                        return response::send_error({
                            .description = concat_tostr(
                                "tracee process died unexpectedly before execveat() without an "
                                "error message: ",
                                res_ok.si.description()
                            ),
                        });
                    } catch (...) {
                        return response::send_error({
                            .description = noexcept_concat(
                                "tracee process died unexpectedly before execveat() without an "
                                "error message: si_code = ",
                                res_ok.si.code,
                                ", si_status = ",
                                res_ok.si.status
                            ),
                        });
                    }
                }
                auto tracee_exec_start_time = *tracee_exec_start_time_opt;
                auto tracee_exec_start_cpu_time_user_usec =
                    *tracee_exec_start_cpu_time_user_usec_opt;
                auto tracee_exec_start_cpu_time_system_usec =
                    *tracee_exec_start_cpu_time_system_usec_opt;
                auto tracee_waitid_time =
                    WONT_THROW(sms::read(shared_mem_state->tracee_waitid_time).value());
                return response::send_ok({
                    .tracee_si = res_ok.si,
                    .tracee_runtime = tracee_waitid_time - tracee_exec_start_time,
                    .tracee_cgroup =
                        {
                            .cpu_time =
                                {
                                    .user_usec = tracee_cpu_times.user_usec -
                                        tracee_exec_start_cpu_time_user_usec,
                                    .system_usec = tracee_cpu_times.system_usec -
                                        tracee_exec_start_cpu_time_system_usec,
                                },
                            .peak_memory_in_bytes = tracee_cgroup_peak_memory_in_bytes,
                        },
                });
            },
            [&](const sms::result::Error& res_err) noexcept {
                return response::send_error({
                    .description = {res_err.description.data(), res_err.description.size()},
                });
            },
            [&](const sms::result::None& /*res_none*/) noexcept {
                try {
                    return response::send_error({
                        .description = concat_tostr(
                            "pid1 process ",
                            pid1_si.description(),
                            " without result or error message"
                        ),
                    });
                } catch (...) {
                    return response::send_error({
                        .description = noexcept_concat(
                            "pid1 process died (si_code = ",
                            pid1_si.code,
                            ", si_status = ",
                            pid1_si.status,
                            ") without result or error message"
                        ),
                    });
                }
            },
        },
        res
    );
}

static void write_file_at(int dirfd, FilePath file_path, StringView data) noexcept {
    auto fd = openat(dirfd, file_path, O_WRONLY | O_TRUNC | O_CLOEXEC);
    if (fd == -1) {
        die_with_error("openat(", file_path, ")");
    }
    if (write_all(fd, data) != data.size()) {
        die_with_error("write(", file_path, ")");
    }
    if (close(fd)) {
        die_with_error("close()");
    }
}

static optional<int>
write_file_at_but_expect_write_error(int dirfd, FilePath file_path, StringView data) noexcept {
    auto fd = openat(dirfd, file_path, O_WRONLY | O_TRUNC | O_CLOEXEC);
    if (fd == -1) {
        die_with_error("openat(", file_path, ")");
    }
    optional<int> res = std::nullopt;
    if (write_all(fd, data) != data.size()) {
        res = errno;
    }
    if (close(fd)) {
        die_with_error("close()");
    }
    return res;
}

template <class T>
static size_t read_file_at_into_number(int dirfd, FilePath file_path) noexcept {
    auto fd = openat(dirfd, file_path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        die_with_error("openat()");
    }
    std::array<char, 24> buff;
    ssize_t len;
    do {
        len = read(fd, buff.data(), buff.size());
    } while (len < 0 && errno == EINTR);
    if (len < 0) {
        die_with_error("read()");
    }
    if (close(fd)) {
        die_with_error("close()");
    }
    auto data = StringView{buff.data(), static_cast<size_t>(len)};
    if (data.empty() || data.back() != '\n') {
        die_with_msg(file_path, ": expected to read newline as the last character");
    }
    data.remove_trailing('\n');
    auto res = str2num<T>(data);
    if (!res) {
        die_with_msg(file_path, ": ", data, " does not convert to number");
    }
    return *res;
}

namespace user_namespace {

struct SetupArgs {
    uid_t outside_uid;
    uid_t inside_uid;
    gid_t outside_gid;
    gid_t inside_gid;
};

void setup(SetupArgs args) noexcept {
    write_file_at(
        -1,
        "/proc/self/uid_map",
        from_unsafe{noexcept_concat(args.inside_uid, ' ', args.outside_uid, " 1")}
    );
    write_file_at(-1, "/proc/self/setgroups", "deny");
    write_file_at(
        -1,
        "/proc/self/gid_map",
        from_unsafe{noexcept_concat(args.inside_gid, ' ', args.outside_gid, " 1")}
    );
}

} // namespace user_namespace

namespace mount_namespace {

struct MountNamespace {};

MountNamespace setup() noexcept { return {}; }

} // namespace mount_namespace

namespace cgroups {

struct Cgroups {
    const int cgroupfs_fd;
    static constexpr auto supervisor_cgroup_path = StaticCStringBuff{"supervisor"};
    static constexpr auto pid1_cgroup_path = StaticCStringBuff{"pid1"};
    static constexpr auto tracee_cgroup_path = StaticCStringBuff{"tracee"};

    const int pid1_cgroup_fd;
    int tracee_cgroup_fd;
    int tracee_cgroup_kill_fd;
    int tracee_cgroup_cpu_stat_fd;

    void assert_nsdelegate_is_active() noexcept;
    void assert_process_cannot_cross_ns_root_dir_boundary() noexcept;
    void assert_controller_interface_files_in_ns_root_dir_are_unwritable() const noexcept;

    void destroy_tracee_cgroup() noexcept;
    void create_and_set_up_tracee_cgroup() noexcept;
    void reset_tracee_cgroup() noexcept;

    void write_tracee_cgroup_process_num_limit(optional<uint32_t> process_num_limit) noexcept;
    void write_tracee_cgroup_memory_limit(optional<uint64_t> memory_limit_in_bytes) noexcept;
    void write_tracee_cgroup_cpu_max(
        optional<request::Request::Cgroup::CpuMaxBandwidth> cpu_max_bandwidth
    ) noexcept;

    [[nodiscard]] cgroups::CpuTimes read_tracee_cgroup_cpu_times() const noexcept;
    [[nodiscard]] uint64_t read_tracee_cgroup_current_memory_usage() const noexcept;
    [[nodiscard]] uint64_t read_tracee_cgroup_peak_memory_usage() const noexcept;

    void set_tracee_limits(const request::Request::Cgroup& cg) noexcept;
};

Cgroups setup(mount_namespace::MountNamespace& /*required to mount cgroup2*/) noexcept {
    // Mount cgroups v2.
    // Mounting cgroups on our own, we are independent of the system cgroup mountpoint. This way is
    // way simpler and we don't have to to deal with mount point shadowing other mount point in
    // /proc/self/mountinfo.

    int cgroupfs_config_fd = fsopen("cgroup2", FSOPEN_CLOEXEC);
    if (cgroupfs_config_fd < 0) {
        die_with_error("fsopen(cgroup2)");
    }
    if (fsconfig(cgroupfs_config_fd, FSCONFIG_SET_FLAG, "nsdelegate", nullptr, 0)) {
        die_with_error("fsconfig(cgroup2, SET_FLAG: nsdelegate)");
    }
    if (fsconfig(cgroupfs_config_fd, FSCONFIG_CMD_CREATE, nullptr, nullptr, 0)) {
        die_with_error("fsconfig(cgroup2, CMD_CREATE)");
    }
    int cgroupfs_fd = fsmount(cgroupfs_config_fd, FSMOUNT_CLOEXEC, 0);
    if (cgroupfs_fd < 0) {
        die_with_error("fsmount(cgroup2)");
    }
    if (close(cgroupfs_config_fd)) {
        die_with_error("close()");
    }
    // Create cgroups
    if (mkdirat(cgroupfs_fd, Cgroups::supervisor_cgroup_path.c_str(), S_0755) ||
        mkdirat(cgroupfs_fd, Cgroups::pid1_cgroup_path.c_str(), S_0755))
    {
        die_with_error("mkdirat()");
    }

    // Disable PSI accounting to reduce the sandboxing overhead
    write_file_at(
        cgroupfs_fd, noexcept_concat(Cgroups::supervisor_cgroup_path, "/cgroup.pressure"), "0"
    );
    write_file_at(cgroupfs_fd, noexcept_concat(Cgroups::pid1_cgroup_path, "/cgroup.pressure"), "0");

    // Move the current process to the supervisor cgroup
    write_file_at(
        cgroupfs_fd, noexcept_concat(Cgroups::supervisor_cgroup_path, "/cgroup.procs"), "0"
    );

    // Enable resource controllers
    write_file_at(cgroupfs_fd, "cgroup.subtree_control", "+pids +memory +cpu");

    int pid1_cgroup_fd = openat(cgroupfs_fd, Cgroups::pid1_cgroup_path.c_str(), O_PATH | O_CLOEXEC);
    if (pid1_cgroup_fd < 0) {
        die_with_error("openat()");
    }

    Cgroups cgs = {
        .cgroupfs_fd = cgroupfs_fd,
        .pid1_cgroup_fd = pid1_cgroup_fd,
        .tracee_cgroup_fd = -1,
        .tracee_cgroup_kill_fd = -1,
        .tracee_cgroup_cpu_stat_fd = -1,
    };

    cgs.create_and_set_up_tracee_cgroup();
    cgs.assert_nsdelegate_is_active();
    return cgs;
}

void Cgroups::assert_nsdelegate_is_active() noexcept {
    assert_process_cannot_cross_ns_root_dir_boundary();
    assert_controller_interface_files_in_ns_root_dir_are_unwritable();
}

// NOLINTNEXTLINE(readability-make-member-function-const)
void Cgroups::assert_process_cannot_cross_ns_root_dir_boundary() noexcept {
    static constexpr auto test_cgroup_path = "test_nsdelegate";
    if (mkdirat(cgroupfs_fd, test_cgroup_path, S_0755)) {
        die_with_error("mkdirat()");
    }
    int test_cgroup_fd = openat(cgroupfs_fd, test_cgroup_path, O_PATH | O_CLOEXEC);
    if (test_cgroup_fd < 0) {
        die_with_error("openat()");
    }

    auto supervisor_pid = getpid();

    int child_pidfd = 0;
    clone_args cl_args = {};
    cl_args.flags = CLONE_PIDFD | CLONE_NEWUSER | CLONE_NEWCGROUP | CLONE_INTO_CGROUP;
    cl_args.pidfd = reinterpret_cast<uint64_t>(&child_pidfd);
    cl_args.exit_signal = 0; // we don't need SIGCHLD
    cl_args.cgroup = static_cast<uint64_t>(test_cgroup_fd);

    auto pid = syscalls::clone3(&cl_args);
    if (pid == -1) {
        die_with_error("clone3()");
    }
    if (pid == 0) {
        // We have to open this file in the child process, otherwise write will succeed.
        int fd = openat(cgroupfs_fd, "cgroup.procs", O_WRONLY | O_TRUNC | O_CLOEXEC);
        // The above open is racy, the parent process could have already died, become waited, and a
        // new process could take its PID, it might even have the file we try to open. Writing to it
        // is unsafe, so we first check if the parent process is still alive.
        if (getppid() != supervisor_pid) {
            // Supervisor process is dead, we might have opened the wrong file (of a totally
            // different process), so we don't want to write to.
            _exit(3);
        }
        if (fd < 0) {
            _exit(2); // We can't use die_with_error() in the child process
        }
        if (write(fd, "0", std::strlen("0")) != -1 || errno != ENOENT) {
            _exit(1);
        }
        _exit(0);
    }
    siginfo_t si;
    if (syscalls::waitid(P_PIDFD, child_pidfd, &si, __WALL | WEXITED, nullptr)) {
        die_with_error("waitid()");
    }
    if (si.si_code != CLD_EXITED || !is_one_of(si.si_status, 0, 1)) {
        die_with_msg("nsdelegate check: the child process died of unexpected reason");
    }
    if (si.si_status != 0) {
        die_with_msg("nsdelegate cgroup2 option is not in action");
    }

    if (close(child_pidfd) || close(test_cgroup_fd)) {
        die_with_error("close()");
    }
    if (unlinkat(cgroupfs_fd, test_cgroup_path, AT_REMOVEDIR)) {
        die_with_error("unlinkat()");
    }
}

void Cgroups::assert_controller_interface_files_in_ns_root_dir_are_unwritable() const noexcept {
    if (write_file_at_but_expect_write_error(cgroupfs_fd, "pids.max", "max") != EPERM) {
        die_with_error("nsdelegate cgroup2 option is not in action");
    }
    if (write_file_at_but_expect_write_error(cgroupfs_fd, "memory.max", "max") != EPERM) {
        die_with_error("nsdelegate cgroup2 option is not in action");
    }
    if (write_file_at_but_expect_write_error(cgroupfs_fd, "cpu.max", "max") != EPERM) {
        die_with_error("nsdelegate cgroup2 option is not in action");
    }
}

void Cgroups::create_and_set_up_tracee_cgroup() noexcept {
    if (mkdirat(cgroupfs_fd, tracee_cgroup_path.c_str(), S_0755)) {
        die_with_error("mkdirat()");
    }
    tracee_cgroup_fd = openat(cgroupfs_fd, tracee_cgroup_path.c_str(), O_PATH | O_CLOEXEC);
    if (tracee_cgroup_fd < 0) {
        die_with_error("openat()");
    }
    tracee_cgroup_kill_fd = openat(tracee_cgroup_fd, "cgroup.kill", O_WRONLY | O_CLOEXEC);
    if (tracee_cgroup_kill_fd < 0) {
        die_with_error("openat()");
    }
    tracee_cgroup_cpu_stat_fd = openat(tracee_cgroup_fd, "cpu.stat", O_RDONLY | O_CLOEXEC);
    if (tracee_cgroup_cpu_stat_fd < 0) {
        die_with_error("openat()");
    }
    // Disable PSI accounting to reduce the sandboxing overhead
    write_file_at(tracee_cgroup_fd, "cgroup.pressure", "0");
}

void Cgroups::destroy_tracee_cgroup() noexcept {
    if (close(tracee_cgroup_fd)) {
        die_with_error("close()");
    }
    tracee_cgroup_fd = -1;
    if (close(tracee_cgroup_kill_fd)) {
        die_with_error("close()");
    }
    tracee_cgroup_kill_fd = -1;
    if (close(tracee_cgroup_cpu_stat_fd)) {
        die_with_error("close()");
    }
    tracee_cgroup_cpu_stat_fd = -1;

    if (unlinkat(cgroupfs_fd, tracee_cgroup_path.c_str(), AT_REMOVEDIR)) {
        die_with_error("unlinkat()");
    }
}

void Cgroups::reset_tracee_cgroup() noexcept {
    destroy_tracee_cgroup();
    create_and_set_up_tracee_cgroup();
}

// NOLINTNEXTLINE(readability-make-member-function-const)
void Cgroups::write_tracee_cgroup_process_num_limit(optional<uint32_t> process_num_limit) noexcept {
    write_file_at(
        tracee_cgroup_fd,
        "pids.max",
        process_num_limit ? StringView{from_unsafe{to_string(*process_num_limit)}} : "max"
    );
}

// NOLINTNEXTLINE(readability-make-member-function-const)
void Cgroups::write_tracee_cgroup_memory_limit(optional<uint64_t> memory_limit_in_bytes) noexcept {
    write_file_at(
        tracee_cgroup_fd,
        "memory.max",
        memory_limit_in_bytes ? StringView{from_unsafe{to_string(*memory_limit_in_bytes)}} : "max"
    );
}

// NOLINTNEXTLINE(readability-make-member-function-const)
void Cgroups::write_tracee_cgroup_cpu_max(
    optional<request::Request::Cgroup::CpuMaxBandwidth> cpu_max_bandwidth
) noexcept {
    write_file_at(
        tracee_cgroup_fd,
        "cpu.max",
        cpu_max_bandwidth ? StringView{from_unsafe{noexcept_concat(
                                cpu_max_bandwidth->max_usec, ' ', cpu_max_bandwidth->period_usec
                            )}}
                          : "max"
    );
}

cgroups::CpuTimes Cgroups::read_tracee_cgroup_cpu_times() const noexcept {
    return read_cpu_times(tracee_cgroup_cpu_stat_fd, [] [[noreturn]] (auto&&... msg) {
        die_with_msg("read_cpu_times(): ", std::forward<decltype(msg)>(msg)...);
    });
}

uint64_t Cgroups::read_tracee_cgroup_current_memory_usage() const noexcept {
    return read_file_at_into_number<uint64_t>(tracee_cgroup_fd, "memory.current");
}

uint64_t Cgroups::read_tracee_cgroup_peak_memory_usage() const noexcept {
    return read_file_at_into_number<uint64_t>(tracee_cgroup_fd, "memory.peak");
}

void Cgroups::set_tracee_limits(const request::Request::Cgroup& cg) noexcept {
    write_tracee_cgroup_process_num_limit(cg.process_num_limit);
    assert(read_tracee_cgroup_current_memory_usage() == 0 && "Needed to not offset limit by this");
    write_tracee_cgroup_memory_limit(cg.memory_limit_in_bytes);
    write_tracee_cgroup_cpu_max(cg.cpu_max_bandwidth);
}

} // namespace cgroups

void setup_uts_namespace() noexcept {
    if (sethostname("", 0)) {
        die_with_error("sethostname()");
    }
    if (setdomainname("", 0)) {
        die_with_error("setdomainname()");
    }
}

namespace capabilities {

void set_and_lock_all_securebits_for_this_and_all_descendant_processes() noexcept {
    if (prctl(
            PR_SET_SECUREBITS,
            /* SECBIT_KEEP_CAPS off */
            SECBIT_KEEP_CAPS_LOCKED | SECBIT_NO_SETUID_FIXUP | SECBIT_NO_SETUID_FIXUP_LOCKED |
                SECBIT_NOROOT | SECBIT_NOROOT_LOCKED | SECBIT_NO_CAP_AMBIENT_RAISE |
                SECBIT_NO_CAP_AMBIENT_RAISE_LOCKED,
            0,
            0,
            0
        ))
    {
        die_with_error("prctl(PR_SET_SECUREBITS)");
    }
}

void set_no_new_privs_for_this_and_all_descendant_processes() noexcept {
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0, 0)) {
        die_with_error("prctl(PR_SET_NO_NEW_PRIVS");
    }
}

} // namespace capabilities

// Waits for pid1 death or read and write shutdown of the other end of the SOCK_FD (we can't wait
// only for the read-close)
void wait_for_pid1_death_or_sock_fd_shutdown(int pid1_pidfd) noexcept {
    std::array<pollfd, 2> pfds;
    auto& sock_fd_pfd = pfds[0];
    auto& pidfd_pfd = pfds[1];
    sock_fd_pfd = {
        .fd = SOCK_FD,
        .events = 0, // wait for POLLHUP i.e. both read and write shutdown of the other end
        .revents = 0,
    };
    pidfd_pfd = {
        .fd = pid1_pidfd,
        .events = POLLIN,
        .revents = 0,
    };
    for (;;) {
        sock_fd_pfd.revents = 0;
        pidfd_pfd.revents = 0;
        int rc = poll(pfds.data(), pfds.size(), -1);
        if (rc == 0 || (rc == -1 && errno == EINTR)) {
            continue;
        }
        if (rc == -1) {
            die_with_error("poll()");
        }
        if (sock_fd_pfd.revents & POLLHUP) {
            // Probably the client process died and we are still alive, but we cannot
            // communicate with the client anymore, so die early to save resources.
            die_with_msg("the other end of the socket was closed");
        }
        if (pidfd_pfd.revents & POLLIN) {
            return; // pid1 process is waitable
        }
    }
}

void main(int argc, char** argv) noexcept {
    check_that_we_do_not_run_as_root();
    auto args = parse_args(argc, argv);
    validate_sock_fd(args.sock_fd);
    initialize_sock_fd_and_error_fd(args.sock_fd);
    set_process_name();
    close_all_stray_file_descriptors();
    replace_stdin_stdout_stderr_with_dev_null();

    namespace sms = communication::supervisor_pid1_tracee;
    auto shared_mem_state_raw = mmap(
        nullptr,
        sizeof(sms::SharedMemState),
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1,
        0
    );
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    if (shared_mem_state_raw == MAP_FAILED) {
        die_with_error("mmap()");
    }
    auto shared_mem_state = sms::initialize(shared_mem_state_raw);
    auto supervisor_pidfd = syscalls::pidfd_open(getpid(), 0);

    auto supervisor_outside_euid = geteuid();
    auto supervisor_outside_egid = getegid();

    if (unshare(
            CLONE_NEWUSER | CLONE_NEWCGROUP | CLONE_NEWNS | CLONE_NEWNET | CLONE_NEWIPC |
            CLONE_NEWUTS
        ))
    {
        die_with_error("unshare()");
    }

    constexpr uid_t SUPERVISOR_USER_NS_INSIDE_UID = 1;
    constexpr uid_t SUPERVISOR_USER_NS_INSIDE_GID = 1;
    user_namespace::setup({
        .outside_uid = supervisor_outside_euid,
        .inside_uid = SUPERVISOR_USER_NS_INSIDE_UID,
        .outside_gid = supervisor_outside_egid,
        .inside_gid = SUPERVISOR_USER_NS_INSIDE_GID,
    });
    auto mount_ns = mount_namespace::setup();
    auto cgroups = cgroups::setup(mount_ns);
    setup_uts_namespace();

    capabilities::set_and_lock_all_securebits_for_this_and_all_descendant_processes();
    capabilities::set_no_new_privs_for_this_and_all_descendant_processes();

    for (;; [&] {
             sms::reset(shared_mem_state);
             cgroups.reset_tracee_cgroup();
         }())
    {
        auto request = recv_request();

        cgroups.set_tracee_limits(request.cgroup);

        int pid1_pidfd = 0;
        clone_args cl_args = {};
        cl_args.flags =
            CLONE_PIDFD | CLONE_INTO_CGROUP | CLONE_NEWPID | CLONE_NEWUSER | CLONE_NEWNS;
        cl_args.pidfd = reinterpret_cast<uint64_t>(&pid1_pidfd);
        cl_args.exit_signal = 0; // we don't need SIGCHLD
        cl_args.cgroup = static_cast<uint64_t>(cgroups.pid1_cgroup_fd);

        auto pid = syscalls::clone3(&cl_args);
        if (pid == -1) {
            die_with_error("clone3()");
        }
        if (pid == 0) {
            constexpr uid_t PID1_USER_NS_INSIDE_UID = 1;
            constexpr uid_t PID1_USER_NS_INSIDE_GID = 1;
            pid1::main({
                .shared_mem_state = shared_mem_state,
                .executable_fd = request.executable_fd,
                .stdin_fd = request.stdin_fd,
                .stdout_fd = request.stdout_fd,
                .stderr_fd = request.stderr_fd,
                .argv = std::move(request.argv),
                .env = std::move(request.env),
                .supervisor_pidfd = supervisor_pidfd,
                .tracee_cgroup_fd = cgroups.tracee_cgroup_fd,
                .tracee_cgroup_kill_fd = cgroups.tracee_cgroup_kill_fd,
                .tracee_cgroup_cpu_stat_fd = cgroups.tracee_cgroup_cpu_stat_fd,
                .linux_namespaces =
                    {
                        .user =
                            {
                                .pid1 =
                                    {
                                        .outside_uid = SUPERVISOR_USER_NS_INSIDE_UID,
                                        .inside_uid = PID1_USER_NS_INSIDE_UID,
                                        .outside_gid = SUPERVISOR_USER_NS_INSIDE_GID,
                                        .inside_gid = PID1_USER_NS_INSIDE_GID,
                                    },

                                .tracee =
                                    {
                                        .outside_uid = PID1_USER_NS_INSIDE_UID,
                                        .inside_uid =
                                            request.linux_namespaces.user.inside_uid.value_or(1000),
                                        .outside_gid = PID1_USER_NS_INSIDE_GID,
                                        .inside_gid =
                                            request.linux_namespaces.user.inside_gid.value_or(1000),
                                    },

                            },
                        .mount =
                            {
                                .operations = request.linux_namespaces.mount.operations,
                                .new_root_mount_path =
                                    request.linux_namespaces.mount.new_root_mount_path,
                            },
                    },
                .prlimit = request.prlimit,
                .time_limit = request.time_limit,
                .cpu_time_limit = request.cpu_time_limit,
                .max_tracee_parallelism = static_cast<decltype(pid1::Args::max_tracee_parallelism)>(
                    request.cgroup.process_num_limit
                        ? meta::min(
                              *request.cgroup.process_num_limit, std::thread::hardware_concurrency()
                          )
                        : std::thread::hardware_concurrency()
                ),
                .tracee_is_restricted_to_single_thread = request.cgroup.process_num_limit == 1,
            });
        }
        close_request_fds(request);
        wait_for_pid1_death_or_sock_fd_shutdown(pid1_pidfd);
        siginfo_t si;
        syscalls::waitid(P_PIDFD, pid1_pidfd, &si, __WALL | WEXITED, nullptr);
        read_shared_mem_state_and_send_response(
            shared_mem_state,
            Si{
                .code = si.si_code,
                .status = si.si_status,
            },
            cgroups.read_tracee_cgroup_cpu_times(),
            cgroups.read_tracee_cgroup_peak_memory_usage()
        );
    }
}

} // namespace sandbox::supervisor

int main(int argc, char** argv) noexcept {
    sandbox::supervisor::main(argc, argv);
    return 0;
}
