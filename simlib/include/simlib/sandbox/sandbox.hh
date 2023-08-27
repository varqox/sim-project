#pragma once

#include <exception>
#include <simlib/sandbox/si.hh>
#include <string>
#include <string_view>
#include <variant>

namespace sandbox {

struct ResultOk {
    int system_result;
};

struct ResultError {
    std::string description;
};

using Result = std::variant<ResultOk, ResultError>;

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
    void send_request(std::string_view shell_command);

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
