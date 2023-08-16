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
#include <simlib/overloaded.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/serialize.hh>
#include <simlib/slice.hh>
#include <sys/stat.h>
#include <type_traits>
#include <utility>
#include <variant>

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
void serialize(
    Writer<phase>& writer, const RequestOptions::LinuxNamespaces::Mount::MountTmpfs& mt
) {
    namespace mount_tmpfs =
        communication::client_supervisor::request::linux_namespaces::mount::mount_tmpfs;
    serialize_as_null_terminated(writer, mt.path);
    writer.write_flags(
        {
            {mt.max_total_size_of_files_in_bytes.has_value(), mount_tmpfs::flags::max_total_size_of_files_in_bytes},
            {mt.inode_limit.has_value(), mount_tmpfs::flags::inode_limit},
            {mt.read_only, mount_tmpfs::flags::read_only},
            {mt.no_exec, mount_tmpfs::flags::no_exec},
        },
        as<mount_tmpfs::flags_t>
    );
    if (mt.max_total_size_of_files_in_bytes) {
        writer
            .write(*mt.max_total_size_of_files_in_bytes, as<mount_tmpfs::max_total_size_of_files_in_bytes_t>);
    }
    if (mt.inode_limit) {
        writer.write(*mt.inode_limit, as<mount_tmpfs::inode_limit_t>);
    }

    if ((mt.root_dir_mode & ACCESSPERMS) != mt.root_dir_mode) {
        THROW("MountTmpfs: invalid root_dir_mode: ", mt.root_dir_mode);
    }
    writer
        .write(mt.root_dir_mode, casted_as<communication::client_supervisor::request::linux_namespaces::mount::mode_t>);
}

template <Phase phase>
void serialize(Writer<phase>& writer, const RequestOptions::LinuxNamespaces::Mount::MountProc& mp) {
    namespace mount_proc =
        communication::client_supervisor::request::linux_namespaces::mount::mount_proc;
    serialize_as_null_terminated(writer, mp.path);
    writer.write_flags(
        {
            {mp.read_only, mount_proc::flags::read_only},
            {mp.no_exec, mount_proc::flags::no_exec},
        },
        as<mount_proc::flags_t>
    );
}

template <Phase phase>
void serialize(Writer<phase>& writer, const RequestOptions::LinuxNamespaces::Mount::BindMount& bm) {
    namespace bind_mount =
        communication::client_supervisor::request::linux_namespaces::mount::bind_mount;
    serialize_as_null_terminated(writer, bm.source);
    serialize_as_null_terminated(writer, bm.dest);
    writer.write_flags(
        {
            {bm.recursive, bind_mount::flags::recursive},
            {bm.read_only, bind_mount::flags::read_only},
            {bm.no_exec, bind_mount::flags::no_exec},
        },
        as<bind_mount::flags_t>
    );
}

template <Phase phase>
void serialize(Writer<phase>& writer, const RequestOptions::LinuxNamespaces::Mount::CreateDir& cd) {
    serialize_as_null_terminated(writer, cd.path);

    if ((cd.mode & ACCESSPERMS) != cd.mode) {
        THROW("CreateDir: invalid mode: ", cd.mode);
    }
    writer
        .write(cd.mode, casted_as<communication::client_supervisor::request::linux_namespaces::mount::mode_t>);
}

template <Phase phase>
void serialize(
    Writer<phase>& writer, const RequestOptions::LinuxNamespaces::Mount::CreateFile& cf
) {
    serialize_as_null_terminated(writer, cf.path);

    if ((cf.mode & ACCESSPERMS) != cf.mode) {
        THROW("CreateFile: invalid mode: ", cf.mode);
    }
    writer
        .write(cf.mode, casted_as<communication::client_supervisor::request::linux_namespaces::mount::mode_t>);
}

template <Phase phase>
void serialize(Writer<phase>& writer, const RequestOptions::LinuxNamespaces::Mount::Operation& op) {
    using communication::client_supervisor::request::linux_namespaces::mount::OperationKind;
    using Mount = RequestOptions::LinuxNamespaces::Mount;
    auto operation_kind = std::visit(
        overloaded{
            [](const Mount::MountTmpfs& /**/) noexcept { return OperationKind::MOUNT_TMPFS; },
            [](const Mount::MountProc& /**/) noexcept { return OperationKind::MOUNT_PROC; },
            [](const Mount::BindMount& /**/) noexcept { return OperationKind::BIND_MOUNT; },
            [](const Mount::CreateDir& /**/) noexcept { return OperationKind::CREATE_DIR; },
            [](const Mount::CreateFile& /**/) noexcept { return OperationKind::CREATE_FILE; },
        },
        op
    );
    writer.write_as_bytes(
        static_cast<std::underlying_type_t<decltype(operation_kind)>>(operation_kind)
    );
    std::visit([&writer](const auto& inner_op) { serialize(writer, inner_op); }, op);
}

template <Phase phase>
void serialize(Writer<phase>& writer, const RequestOptions::LinuxNamespaces::Mount& m) {
    namespace mount = communication::client_supervisor::request::linux_namespaces::mount;
    writer.write(m.operations.size(), casted_as<mount::operations_len_t>);
    for (const auto& op : m.operations) {
        serialize(writer, op);
    }
    if (m.new_root_mount_path) {
        writer
            .write(m.new_root_mount_path->size() + 1, casted_as<mount::new_root_mount_path_len_t>);
        serialize_as_null_terminated(writer, *m.new_root_mount_path);
    } else {
        writer.write(0, casted_as<mount::new_root_mount_path_len_t>);
    }
}

template <Phase phase>
void serialize(Writer<phase>& writer, const RequestOptions::LinuxNamespaces& linux_namespaces) {
    serialize(writer, linux_namespaces.user);
    serialize(writer, linux_namespaces.mount);
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

template <Phase phase>
void serialize(Writer<phase>& writer, const RequestOptions::Prlimit& pr) {
    namespace prlimit = communication::client_supervisor::request::prlimit;
    writer.write_flags({
            {pr.max_address_space_size_in_bytes.has_value(), prlimit::mask::max_address_space_size_in_bytes},
            {pr.max_core_file_size_in_bytes.has_value(), prlimit::mask::max_core_file_size_in_bytes},
            {pr.cpu_time_limit_in_seconds.has_value(), prlimit::mask::cpu_time_limit_in_seconds},
        }, as<prlimit::mask_t>);
    if (pr.max_address_space_size_in_bytes) {
        writer
            .write(*pr.max_address_space_size_in_bytes, as<prlimit::max_address_space_size_in_bytes_t>);
    }
    if (pr.max_core_file_size_in_bytes) {
        writer.write(*pr.max_core_file_size_in_bytes, as<prlimit::max_core_file_size_in_bytes_t>);
    }
    if (pr.cpu_time_limit_in_seconds) {
        writer.write(*pr.cpu_time_limit_in_seconds, as<prlimit::cpu_time_limit_in_seconds_t>);
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
        serialize(writer, options.prlimit);
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
