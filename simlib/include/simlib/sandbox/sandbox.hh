#pragma once

#include <chrono>
#include <cstdint>
#include <exception>
#include <optional>
#include <simlib/file_perms.hh>
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

        struct Mount {
            struct MountTmpfs {
                std::string_view path;
                // Will be rounded up to page size multiple; 0 rounds up to page size due to
                // technical limitations.
                std::optional<uint64_t> max_total_size_of_files_in_bytes = 0;
                std::optional<uint64_t> inode_limit = 0;
                mode_t root_dir_mode = S_0755;
                bool read_only = true; // make the mount read-only
                bool no_exec = true; // do not allow programs to be executed on this mount
            };

            struct MountProc {
                std::string_view path;
                bool read_only = true; // make the mount read-only
                bool no_exec = true; // do not allow programs to be executed on this mount
            };

            struct BindMount {
                std::string_view source; // path to file or directory to bind mount
                std::string_view dest; // path at which to bind mount
                bool recursive = true; // create a recursive bind mount
                bool read_only = true; // make the mount read-only
                bool no_exec = true; // do not allow programs to be executed from this mount
            };

            struct CreateDir {
                std::string_view path;
                mode_t mode = S_0755;
            };

            struct CreateFile {
                std::string_view path;
                mode_t mode = S_0644;
            };

            using Operation = std::variant<MountTmpfs, MountProc, BindMount, CreateDir, CreateFile>;

            Slice<Operation> operations = {};
            std::optional<std::string_view> new_root_mount_path = std::nullopt;
        } mount = {};
    } linux_namespaces = {};

    struct Cgroup {
        // Every process or thread counts as 1
        std::optional<uint32_t> process_num_limit = std::nullopt;
        std::optional<uint64_t> memory_limit_in_bytes = std::nullopt;
    } cgroup = {};

    struct Prlimit {
        std::optional<uint64_t> max_address_space_size_in_bytes = std::nullopt; // RLIMIT_AS
        std::optional<uint64_t> max_core_file_size_in_bytes = std::nullopt; // RLIMIT_CORE
        std::optional<uint64_t> cpu_time_limit_in_seconds = std::nullopt; // RLIMIT_CPU
        std::optional<uint64_t> max_file_size_in_bytes = std::nullopt; // RLIMIT_FSIZE
        std::optional<uint64_t> file_descriptors_num_limit = std::nullopt; // RLIMIT_NOFILE
        std::optional<uint64_t> max_stack_size_in_bytes = std::nullopt; // RLIMIT_STACK
    } prlimit = {};

    std::optional<std::chrono::nanoseconds> time_limit = std::nullopt;
};

namespace result {
struct Ok {
    Si si;
    std::chrono::nanoseconds runtime; // from CLOCK_MONOTONIC_RAW

    struct Cgroup {
        struct CpuTime {
            std::chrono::microseconds user;
            std::chrono::microseconds system;

            [[nodiscard]] std::chrono::microseconds total() const noexcept { return user + system; }
        } cpu_time;

        uint64_t peak_memory_in_bytes;
    } cgroup;
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
