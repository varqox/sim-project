#include "../../communication/client_supervisor.hh"
#include "request.hh"

#include <optional>
#include <simlib/array_vec.hh>
#include <simlib/deserialize.hh>
#include <simlib/string_view.hh>

using deserialize::from;
using deserialize::Reader;
using std::optional;

namespace sandbox::supervisor::request {

StringBase<char> deserialize_null_terminated_string(Reader& reader) {
    auto len_opt = reader.find_byte(std::byte{0});
    if (!len_opt) {
        THROW("could not find null byte in the remaining bytes to deserialize");
    }
    return {reinterpret_cast<char*>(reader.extract_bytes(*len_opt + 1)), *len_opt};
}

CStringView deserialize_cstring_view(Reader& reader) {
    auto str = deserialize_null_terminated_string(reader);
    return CStringView{str.data(), str.size()};
}

void deserialize(Reader& reader, Request::LinuxNamespaces::User& u) {
    namespace user = communication::client_supervisor::request::linux_namespaces::user;
    bool has_inside_uid;
    bool has_inside_gid;
    reader.read_flags({
        {has_inside_uid, user::mask::inside_uid},
        {has_inside_gid, user::mask::inside_gid},
    }, from<user::mask_t>);
    reader.read_optional_if(u.inside_uid, from<user::inside_uid_t>, has_inside_uid);
    reader.read_optional_if(u.inside_gid, from<user::inside_gid_t>, has_inside_gid);
}

void deserialize(Reader& reader, Request::LinuxNamespaces& linux_namespaces) {
    deserialize(reader, linux_namespaces.user);
}

void deserialize(Reader& reader, Request::Cgroup& cg) {
    namespace cgroup = communication::client_supervisor::request::cgroup;
    bool has_process_num_limit;
    bool has_memory_limit_in_bytes;
    reader.read_flags({
        {has_process_num_limit, cgroup::mask::process_num_limit},
        {has_memory_limit_in_bytes, cgroup::mask::memory_limit_in_bytes},
    }, from<cgroup::mask_t>);
    reader.read_optional_if(
        cg.process_num_limit, from<cgroup::process_num_limit_t>, has_process_num_limit
    );
    reader.read_optional_if(
        cg.memory_limit_in_bytes, from<cgroup::memory_limit_in_bytes_t>, has_memory_limit_in_bytes
    );
}

void deserialize(Reader& reader, ArrayVec<int, 253>& fds, Request& req) {
    namespace request = communication::client_supervisor::request;
    size_t fds_taken = 0;
    auto take_fd = [&fds, &fds_taken]() mutable {
        if (fds_taken == fds.size()) {
            THROW("received too little file descriptors");
        }
        return fds[fds_taken++];
    };
    req.executable_fd = take_fd();
    {
        namespace fds = request::fds;
        bool sending_stdin_fd;
        bool sending_stdout_fd;
        bool sending_stderr_fd;
        reader.read_flags({
            {sending_stdin_fd, fds::mask::sending_stdin_fd},
            {sending_stdout_fd, fds::mask::sending_stdout_fd},
            {sending_stderr_fd, fds::mask::sending_stderr_fd},
        }, from<fds::mask_t>);
        req.stdin_fd = sending_stdin_fd ? optional{take_fd()} : std::nullopt;
        req.stdout_fd = sending_stdout_fd ? optional{take_fd()} : std::nullopt;
        req.stderr_fd = sending_stderr_fd ? optional{take_fd()} : std::nullopt;
    }
    if (fds_taken != fds.size()) {
        THROW("received too many file descriptors");
    }

    req.argv.resize(reader.read<size_t>(from<request::argv_len_t>) + 1);
    for (size_t i = 0; i < req.argv.size() - 1; ++i) {
        req.argv[i] = deserialize_null_terminated_string(reader).data();
    }
    req.argv.back() = nullptr; // for execveat()

    req.env.resize(reader.read<size_t>(from<request::env_len_t>) + 1);
    for (size_t i = 0; i < req.env.size() - 1; ++i) {
        req.env[i] = deserialize_null_terminated_string(reader).data();
    }
    req.env.back() = nullptr; // for execveat()

    deserialize(reader, req.linux_namespaces);
    deserialize(reader, req.cgroup);
}

} // namespace sandbox::supervisor::request
