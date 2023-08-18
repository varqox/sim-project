#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <sys/types.h>

namespace sandbox::communication::client_supervisor {

namespace request {

using mask_t = uint8_t;

namespace mask {
static constexpr mask_t sending_stdin_fd = 1;
static constexpr mask_t sending_stdout_fd = 2;
static constexpr mask_t sending_stderr_fd = 4;
} // namespace mask

namespace linux_namespaces::user {

using mask_t = uint8_t;

namespace mask {
static constexpr mask_t inside_uid = 1;
static constexpr mask_t inside_gid = 2;
}; // namespace mask

using serialized_inside_uid_t = uint32_t;
using serialized_inside_gid_t = uint32_t;
static_assert( // NOLINTNEXTLINE(misc-redundant-expression)
    std::numeric_limits<serialized_inside_uid_t>::min() <= std::numeric_limits<uid_t>::min()
);
static_assert( // NOLINTNEXTLINE(misc-redundant-expression)
    std::numeric_limits<serialized_inside_uid_t>::max() >= std::numeric_limits<uid_t>::max()
);
static_assert( // NOLINTNEXTLINE(misc-redundant-expression)
    std::numeric_limits<serialized_inside_gid_t>::min() <= std::numeric_limits<gid_t>::min()
);
static_assert( // NOLINTNEXTLINE(misc-redundant-expression)
    std::numeric_limits<serialized_inside_gid_t>::max() >= std::numeric_limits<gid_t>::max()
);
} // namespace linux_namespaces::user

namespace cgroup {

using mask_t = uint8_t;

namespace mask {
static constexpr mask_t process_num_limit = 1;
static constexpr mask_t memory_limit_in_bytes = 2;
} // namespace mask

using serialized_process_num_limit_t = uint32_t;
using serialized_memory_limit_in_bytes_t = uint64_t;

} // namespace cgroup

template <class T, class MaskT, class SerializedT>
void serialize_into(
    const std::optional<T>& val, MaskT& mask, SerializedT& serialized_val, MaskT flag
) {
    if (val) {
        mask |= flag;
        serialized_val = SerializedT{*val}; // prevent narrowing conversion
    } else {
        mask &= ~flag;
        serialized_val = {};
    }
}

template <class T, class MaskT, class SerializedT>
void deserialize_from(std::optional<T>& x, MaskT& mask, SerializedT& serialized_val, MaskT flag) {
    if (mask & flag) {
        x = T{serialized_val}; // prevent narrowing conversion
    } else {
        x = std::nullopt;
    }
}

using serialized_argv_len_t = uint32_t;
using serialized_env_len_t = uint32_t;

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

} // namespace response

} // namespace sandbox::communication::client_supervisor
