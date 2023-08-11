#pragma once

#include <chrono>
#include <exception>
#include <optional>
#include <simlib/sandbox/si.hh>
#include <simlib/slice.hh>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <variant>

namespace sandbox {

struct RequestOptions {
    std::optional<int> stdin_fd = std::nullopt;
    std::optional<int> stdout_fd = std::nullopt;
    std::optional<int> stderr_fd = std::nullopt;
    Slice<std::string_view> env = {};

    struct LinuxNamespaces {
        struct User {
            std::optional<uid_t> inside_uid = std::nullopt; // nullopt will leave it the same as the
                                                            // outside effective user ID (euid)
            std::optional<gid_t> inside_gid = std::nullopt; // nullopt will leave it the same as the
                                                            // outside effective group ID (egid)
        } user = {};
    } linux_namespaces = {};
};

namespace result {
struct Ok {
    Si si;
    std::chrono::nanoseconds runtime; // from CLOCK_MONOTONIC_RAW
};

struct Error {
    std::string description;
};
} // namespace result

using Result = std::variant<result::Ok, result::Error>;

class SupervisorConnection {
    int sock_fd; // may be closed later than pidfd and error fd because of the unread responses to
                 // the already processed requests
    int supervisor_pidfd;
    int supervisor_error_fd;
    int uncaught_exceptions_in_constructor = std::uncaught_exceptions();

    SupervisorConnection(int sock_fd, int supervisor_pidfd, int supervisor_error_fd) noexcept
    : sock_fd{sock_fd}
    , supervisor_pidfd{supervisor_pidfd}
    , supervisor_error_fd{supervisor_error_fd} {}

public:
    // Spawns supervisor process. Throws on error.
    friend SupervisorConnection spawn_supervisor();

    SupervisorConnection(const SupervisorConnection&) = delete;
    SupervisorConnection(SupervisorConnection&&) noexcept = delete;
    SupervisorConnection& operator=(const SupervisorConnection&) = delete;
    SupervisorConnection& operator=(SupervisorConnection&&) noexcept = delete;
    // Cancels pending and in-progress requests
    ~SupervisorConnection() noexcept(false);

    /// Sends the request and returns immediately without waiting for the request to complete.
    /// Multiple requests can be send before awaiting completion with
    /// await_result() - in the same order.
    /// Throws if there is any error with the supervisor.
    void send_request(int executable_fd, Slice<std::string_view> argv, const RequestOptions&);

    /// Sends the request and returns immediately without waiting for the request to complete.
    /// Multiple requests can be send before awaiting completion with
    /// await_result() - in the same order.
    /// Throws if there is any error with the supervisor.
    void send_request(Slice<std::string_view> argv, const RequestOptions& options = {});

    /// Throws if there is any error with the supervisor.
    Result await_result();

private:
    void kill_and_wait_supervisor() noexcept;

    // Throws on any error, returns Si of the supervisor
    Si kill_and_wait_supervisor_and_receive_errors();

    [[nodiscard]] bool supervisor_is_dead_and_waited() const noexcept {
        return supervisor_pidfd == -1;
    }

    void mark_supervisor_is_dead_and_waited() noexcept { supervisor_pidfd = -1; }
};

SupervisorConnection spawn_supervisor();

} // namespace sandbox
