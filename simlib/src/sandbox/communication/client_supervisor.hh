#pragma once

#include <cstdint>

namespace sandbox::communication::client_supervisor {

namespace request {
using shell_command_len_t = uint32_t;
} // namespace request

namespace response {

using error_len_t = uint32_t;

struct Ok {
    int system_result;
};

} // namespace response

} // namespace sandbox::communication::client_supervisor
