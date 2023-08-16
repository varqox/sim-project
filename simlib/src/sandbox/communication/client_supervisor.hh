#pragma once

#include <cstdint>

namespace sandbox::communication::client_supervisor {

namespace request {

using mask_t = uint8_t;

namespace mask {
static constexpr mask_t sending_stdin_fd = 1;
static constexpr mask_t sending_stdout_fd = 2;
static constexpr mask_t sending_stderr_fd = 4;
} // namespace mask

using serialized_argv_len_t = uint32_t;
using serialized_env_len_t = uint32_t;

} // namespace request

namespace response {

using error_len_t = uint32_t;

namespace time {
using sec_t = int64_t;
using nsec_t = uint32_t;
} // namespace time

namespace si {
using code_t = int32_t;
using status_t = int32_t;
} // namespace si

} // namespace response

} // namespace sandbox::communication::client_supervisor
