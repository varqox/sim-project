#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <sys/types.h>
#include <type_traits>

namespace sandbox::communication::client_supervisor {
template <class A, class B>
constexpr inline bool is_superset_of = std::is_integral_v<A> && std::is_integral_v<B> &&
    std::numeric_limits<A>::min() <= std::numeric_limits<B>::min() &&
    std::numeric_limits<A>::max() >= std::numeric_limits<B>::max();

namespace request {
using body_len_t = uint64_t;

namespace fds {

using mask_t = uint8_t;

namespace mask {
static constexpr mask_t sending_stdin_fd = 1 << 0;
static constexpr mask_t sending_stdout_fd = 1 << 1;
static constexpr mask_t sending_stderr_fd = 1 << 2;
static constexpr mask_t sending_seccomp_bpf_fd = 1 << 3;
} // namespace mask

} // namespace fds

using argv_len_t = uint32_t;
using env_len_t = uint32_t;

namespace linux_namespaces {

namespace user {
using mask_t = uint8_t;

namespace mask {
static constexpr mask_t inside_uid = 1 << 0;
static constexpr mask_t inside_gid = 1 << 1;
}; // namespace mask

using inside_uid_t = uint32_t;
static_assert(is_superset_of<inside_uid_t, uid_t>);
using inside_gid_t = uint32_t;
static_assert(is_superset_of<inside_gid_t, gid_t>);
} // namespace user

namespace mount {

using mode_t = uint16_t;

enum class OperationKind : uint8_t {
    MOUNT_TMPFS = 1,
    MOUNT_PROC = 2,
    BIND_MOUNT = 3,
    CREATE_DIR = 4,
    CREATE_FILE = 5,
};

namespace mount_tmpfs {
using flags_t = uint8_t;

namespace flags {
static constexpr flags_t max_total_size_of_files_in_bytes = 1 << 0;
static constexpr flags_t inode_limit = 1 << 1;
static constexpr flags_t read_only = 1 << 2;
static constexpr flags_t no_exec = 1 << 3;
} // namespace flags

using max_total_size_of_files_in_bytes_t = uint64_t;
using inode_limit_t = uint64_t;
} // namespace mount_tmpfs

namespace mount_proc {

using flags_t = uint8_t;

namespace flags {
static constexpr flags_t read_only = 1 << 0;
static constexpr flags_t no_exec = 1 << 1;
} // namespace flags
} // namespace mount_proc

namespace bind_mount {
using flags_t = uint8_t;

namespace flags {
static constexpr flags_t recursive = 1 << 0;
static constexpr flags_t read_only = 1 << 1;
static constexpr flags_t no_exec = 1 << 2;
} // namespace flags
} // namespace bind_mount

using operations_len_t = uint32_t;
using new_root_mount_path_len_t = uint32_t;

} // namespace mount

} // namespace linux_namespaces

namespace cgroup {

using mask_t = uint8_t;

namespace mask {
static constexpr mask_t process_num_limit = 1 << 0;
static constexpr mask_t memory_limit_in_bytes = 1 << 1;
static constexpr mask_t cpu_max_bandwidth = 1 << 2;
} // namespace mask

using process_num_limit_t = uint32_t;
using memory_limit_in_bytes_t = uint64_t;

namespace cpu_max_bandwidth {
using max_usec_t = uint32_t;
using period_usec_t = uint32_t;
} // namespace cpu_max_bandwidth

} // namespace cgroup

namespace prlimit {

using mask_t = uint8_t;

namespace mask {
static constexpr mask_t max_address_space_size_in_bytes = 1 << 0;
static constexpr mask_t max_core_file_size_in_bytes = 1 << 1;
static constexpr mask_t cpu_time_limit_in_seconds = 1 << 2;
static constexpr mask_t max_file_size_in_bytes = 1 << 3;
static constexpr mask_t file_descriptors_num_limit = 1 << 4;
static constexpr mask_t max_stack_size_in_bytes = 1 << 5;
} // namespace mask

using max_address_space_size_in_bytes_t = uint64_t;
using max_core_file_size_in_bytes_t = uint64_t;
using cpu_time_limit_in_seconds_t = uint64_t;
using max_file_size_in_bytes_t = uint64_t;
using file_descriptors_num_limit_t = uint64_t;
using max_stack_size_in_bytes_t = uint64_t;

} // namespace prlimit

using time_limit_sec_t = int64_t;
using time_limit_nsec_t = uint32_t;

using cpu_time_limit_sec_t = int64_t;
using cpu_time_limit_nsec_t = uint32_t;

} // namespace request

namespace response {

using error_len_t = uint32_t;

namespace time {
using sec_t = uint64_t;
using nsec_t = uint32_t;
} // namespace time

namespace si {
using code_t = int32_t;
using status_t = int32_t;
} // namespace si

namespace cgroup {
using usec_t = uint64_t;
using peak_memory_in_bytes_t = uint64_t;
} // namespace cgroup

} // namespace response

} // namespace sandbox::communication::client_supervisor
