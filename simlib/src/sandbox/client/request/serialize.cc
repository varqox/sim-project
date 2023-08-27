#include "../../communication/client_supervisor.hh"
#include "serialize.hh"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>
#include <simlib/array_vec.hh>
#include <simlib/contains.hh>
#include <simlib/macros/throw.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/serialize.hh>
#include <simlib/slice.hh>
#include <utility>

using serialize::as;
using serialize::casted_as;
using serialize::Phase;
using serialize::Writer;

namespace sandbox::client::request {

template <Phase phase>
void serialize_as_null_terminated(Writer<phase>& writer, std::string_view str) {
    if (contains(str, '\0')) {
        THROW("string cannot contain null character, here string = ", str);
    }
    writer.write_bytes(str.data(), str.size());
    writer.write_as_bytes('\0');
}

template <Phase phase>
void serialize(Writer<phase>& writer, const RequestOptions::LinuxNamespaces::User& u) {
    namespace user = communication::client_supervisor::request::linux_namespaces::user;
    writer.write_flags({
            {u.inside_uid.has_value(), user::mask::inside_uid},
            {u.inside_gid.has_value(), user::mask::inside_gid},
        }, as<user::mask_t>);
    if (u.inside_uid) {
        writer.write(*u.inside_uid, as<user::inside_uid_t>);
    }
    if (u.inside_gid) {
        writer.write(*u.inside_gid, as<user::inside_gid_t>);
    }
}

template <Phase phase>
void serialize(Writer<phase>& writer, const RequestOptions::LinuxNamespaces& linux_namespaces) {
    serialize(writer, linux_namespaces.user);
}

template <Phase phase>
void serialize(Writer<phase>& writer, const RequestOptions::Cgroup& cg) {
    namespace cgroup = communication::client_supervisor::request::cgroup;
    writer.write_flags({
            {cg.process_num_limit.has_value(), cgroup::mask::process_num_limit},
            {cg.memory_limit_in_bytes.has_value(), cgroup::mask::memory_limit_in_bytes},
        }, as<cgroup::mask_t>);
    if (cg.process_num_limit) {
        writer.write(*cg.process_num_limit, as<cgroup::process_num_limit_t>);
    }
    if (cg.memory_limit_in_bytes) {
        writer.write(*cg.memory_limit_in_bytes, as<cgroup::memory_limit_in_bytes_t>);
    }
}

SerializedReuest
serialize(int executable_fd, Slice<std::string_view> argv, const RequestOptions& options) {
    namespace request = communication::client_supervisor::request;
    auto fds = ArrayVec<int, 4>{executable_fd};
    if (options.stdin_fd) {
        fds.emplace(*options.stdin_fd);
    }
    if (options.stdout_fd) {
        fds.emplace(*options.stdout_fd);
    }
    if (options.stderr_fd) {
        fds.emplace(*options.stderr_fd);
    }

    auto do_serialize = [&](auto& writer) {
        namespace fds = request::fds;
        writer.write_flags({
            {options.stdin_fd.has_value(), fds::mask::sending_stdin_fd},
            {options.stdout_fd.has_value(), fds::mask::sending_stdout_fd},
            {options.stderr_fd.has_value(), fds::mask::sending_stderr_fd},
        }, as<fds::mask_t>);

        writer.write(argv.size(), casted_as<request::argv_len_t>);
        for (auto const& arg : argv) {
            serialize_as_null_terminated(writer, arg);
        }

        writer.write(options.env.size(), casted_as<request::env_len_t>);
        for (auto const& str : options.env) {
            serialize_as_null_terminated(writer, str);
        }

        serialize(writer, options.linux_namespaces);
        serialize(writer, options.cgroup);
    };

    // Header
    Writer<Phase::CountLen> count_writer;
    do_serialize(count_writer);
    request::body_len_t body_len = count_writer.written_bytes_num();
    std::array<std::byte, sizeof(body_len)> header;
    std::memcpy(header.data(), &body_len, sizeof(body_len));
    // Body
    auto body = std::make_unique<std::byte[]>(body_len);
    auto writer = Writer<Phase::Serialize>{body.get(), body_len};
    do_serialize(writer);
    assert(writer.remaining_size() == 0);

    return {
        .fds = std::move(fds),
        .header = header,
        .body = std::move(body),
        .body_len = body_len,
    };
}

} // namespace sandbox::client::request
