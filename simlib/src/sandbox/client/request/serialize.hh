#include "../../communication/client_supervisor.hh"

#include <array>
#include <memory>
#include <simlib/array_vec.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/slice.hh>
#include <string_view>
#include <variant>

namespace sandbox::client::request {

struct SerializedReuest {
    ArrayVec<int, 7> fds;
    std::array<std::byte, sizeof(communication::client_supervisor::request::body_len_t)> header;
    std::unique_ptr<std::byte[]> body;
    size_t body_len;
};

SerializedReuest serialize(
    int result_fd,
    int kill_fd,
    std::variant<int, std::string_view> executable,
    Slice<std::string_view> argv,
    const RequestOptions& options
);

} // namespace sandbox::client::request
