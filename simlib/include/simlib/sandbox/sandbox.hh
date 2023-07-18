#pragma once

#include <exception>

namespace sandbox {

class SupervisorConnection {
    int sock_fd;
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
    ~SupervisorConnection() noexcept(false);

private:
    void kill_and_wait_supervisor() noexcept;

    // Throws on any error
    void kill_and_wait_supervisor_and_receive_errors();
};

SupervisorConnection spawn_supervisor();

} // namespace sandbox
