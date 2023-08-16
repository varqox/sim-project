#include "../communication/client_supervisor.hh"
#include "../communication/supervisor_tracee.hh"
#include "../do_die_with_error.hh"
#include "../tracee/tracee.hh"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <optional>
#include <simlib/array_vec.hh>
#include <simlib/deserializer.hh>
#include <simlib/sandbox/si.hh>
#include <simlib/socket_stream_ext.hh>
#include <simlib/string_transform.hh>
#include <simlib/string_view.hh>
#include <simlib/syscalls.hh>
#include <simlib/timespec_arithmetic.hh>
#include <string_view>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using std::optional;
using std::unique_ptr;
using std::vector;

namespace sandbox::supervisor {

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

namespace request = communication::client_supervisor::request;

struct Request {
    int executable_fd;
    optional<int> stdin_fd;
    optional<int> stdout_fd;
    optional<int> stderr_fd;
    unique_ptr<char[]> argv_env_buff;
    vector<char*> argv; // with a trailing nullptr element
    vector<char*> env; // with a trailing nullptr element
};

Request recv_request() noexcept {
    Request req;
    // Receive header
    request::mask_t mask;
    request::serialized_argv_len_t serialized_argv_len;
    request::serialized_env_len_t serialized_env_len;
    std::byte header_buff[sizeof(mask) + sizeof(serialized_argv_len) + sizeof(serialized_env_len)];
    auto fds = recv_request_header_with_fds(header_buff, sizeof(header_buff));
    Deserializer{header_buff, sizeof(header_buff)}
        .deserialize_bytes_as(mask)
        .deserialize_bytes_as(serialized_argv_len)
        .deserialize_bytes_as(serialized_env_len);

    if (fds.size() < 1) {
        die_with_msg(
            "recv_request(): missing executable file descriptor alongside the request header"
        );
    }
    req.executable_fd = fds[0];

    size_t fds_taken = 1;
    auto take_fd = [&fds, &fds_taken]() mutable noexcept {
        if (fds_taken >= fds.size()) {
            die_with_msg(
                "recv_request(): too little file descriptors sent alongside the request header"
            );
        }
        return fds[fds_taken++];
    };
    if (mask & request::mask::sending_stdin_fd) {
        req.stdin_fd = take_fd();
    }
    if (mask & request::mask::sending_stdout_fd) {
        req.stdout_fd = take_fd();
    }
    if (mask & request::mask::sending_stderr_fd) {
        req.stderr_fd = take_fd();
    }
    if (fds_taken != fds.size()) {
        die_with_msg("recv_request(): too many file descriptors sent alongside the request header");
    }

    auto buff_len = serialized_argv_len + serialized_env_len;
    try {
        req.argv_env_buff = std::make_unique<char[]>(buff_len);
    } catch (...) {
        die_with_msg("recv_request(): failed to allocate memory (", buff_len, " bytes)");
    }
    // Receive argv and env
    if (recv_exact(SOCK_FD, req.argv_env_buff.get(), buff_len, 0)) {
        die_with_error("recv()");
    }

    auto parse_null_str_array = [](vector<char*>& vec, MutDeserializer deser, StringView name
                                ) noexcept {
        if (deser.size() > 0 && deser[deser.size() - 1] != std::byte{0}) {
            die_with_msg("recv_request(): last byte of serialized ", name, " is not null");
        }
        // We will add a nullptr arg, so we don't need to reallocate later for execveat()
        size_t vec_size = std::count(deser.data(), deser.data() + deser.size(), std::byte{0}) + 1;
        try {
            vec.reserve(vec_size);
        } catch (...) {
            die_with_msg(
                "recv_request(): failed to allocate memory (", vec_size * sizeof(vec[0]), " bytes)"
            );
        }
        while (!deser.is_empty()) {
            vec.emplace_back(reinterpret_cast<char*>(deser.data()));
            size_t len = 0;
            while (deser[len] != std::byte{0}) {
                ++len;
            }
            deser.extract_bytes(len + 1); // including null byte
        }
        vec.emplace_back(nullptr); // for execveat()
        assert(vec.size() == vec_size);
    };

    MutDeserializer deser{req.argv_env_buff.get(), buff_len};
    parse_null_str_array(req.argv, deser.extract_bytes(serialized_argv_len), "argv");
    parse_null_str_array(req.env, deser.extract_bytes(serialized_env_len), "env");
    assert(deser.size() == 0);

    return req;
}

void close_request_fds(const Request& req) noexcept {
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
            static_cast<response::time::nsec_t>(resp.tracee_runtime.tv_nsec)
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

[[nodiscard]] timespec get_current_time() noexcept {
    timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts)) {
        die_with_error("clock_gettime()");
    }
    return ts;
}

void read_shared_mem_state_and_send_response(
    volatile communication::supervisor_tracee::SharedMemState* shared_mem_state,
    Si tracee_si,
    timespec tracee_waited_time
) noexcept {
    namespace sms = communication::supervisor_tracee;
    auto error = sms::read_error(shared_mem_state);
    auto error_sv = error ? optional{std::string_view{error->data(), error->size()}} : std::nullopt;

    auto tracee_exec_start_time = sms::read(shared_mem_state->tracee_exec_start_time);
    if (!tracee_exec_start_time) {
        return response::send_error({
            .description = error_sv ? *error_sv : "unexpected death before execve()",
        });
    }

    if (error_sv) {
        return response::send_error({
            .description = *error_sv,
        });
    }

    response::send_ok({
        .tracee_si = tracee_si,
        .tracee_runtime = tracee_waited_time - *tracee_exec_start_time,
    });
}

void main(int argc, char** argv) noexcept {
    auto args = parse_args(argc, argv);
    validate_sock_fd(args.sock_fd);
    initialize_sock_fd_and_error_fd(args.sock_fd);
    close_all_stray_file_descriptors();
    replace_stdin_stdout_stderr_with_dev_null();

    namespace sms = communication::supervisor_tracee;
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

    for (;; sms::reset(shared_mem_state)) {
        auto request = recv_request();

        int tracee_pidfd = 0;
        clone_args cl_args = {};
        cl_args.flags = CLONE_PIDFD;
        cl_args.pidfd = reinterpret_cast<uint64_t>(&tracee_pidfd);
        cl_args.exit_signal = 0; // we don't need SIGCHLD
        auto pid = syscalls::clone3(&cl_args);
        if (pid == -1) {
            die_with_error("clone3()");
        }
        if (pid == 0) {
            tracee::main({
                .supervisor_shared_mem_state = shared_mem_state,
                .executable_fd = request.executable_fd,
                .stdin_fd = request.stdin_fd,
                .stdout_fd = request.stdout_fd,
                .stderr_fd = request.stderr_fd,
                .argv = std::move(request.argv),
                .env = std::move(request.env),
            });
        }
        close_request_fds(request);
        siginfo_t si;
        syscalls::waitid(P_PIDFD, tracee_pidfd, &si, __WALL | WEXITED, nullptr);
        timespec tracee_waited_time = get_current_time();
        read_shared_mem_state_and_send_response(
            shared_mem_state,
            Si{
                .code = si.si_code,
                .status = si.si_status,
            },
            tracee_waited_time
        );
    }
}

} // namespace sandbox::supervisor

int main(int argc, char** argv) noexcept {
    sandbox::supervisor::main(argc, argv);
    return 0;
}
