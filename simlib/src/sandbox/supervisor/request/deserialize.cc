#include "../../communication/client_supervisor.hh"
#include "request.hh"

#include <ctime>
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

void deserialize(Reader& reader, Request::LinuxNamespaces::Mount::MountTmpfs& mt) {
    namespace mount_tmpfs =
        communication::client_supervisor::request::linux_namespaces::mount::mount_tmpfs;
    mt.path = deserialize_cstring_view(reader);
    bool has_max_total_size_of_files_in_bytes;
    bool has_inode_limit;
    reader.read_flags(
        {
            {has_max_total_size_of_files_in_bytes, mount_tmpfs::flags::max_total_size_of_files_in_bytes},
            {has_inode_limit, mount_tmpfs::flags::inode_limit},
            {mt.read_only, mount_tmpfs::flags::read_only},
            {mt.no_exec, mount_tmpfs::flags::no_exec},
        },
        from<mount_tmpfs::flags_t>
    );
    reader.read_optional_if(
        mt.max_total_size_of_files_in_bytes,
        from<mount_tmpfs::max_total_size_of_files_in_bytes_t>,
        has_max_total_size_of_files_in_bytes
    );
    reader.read_optional_if(mt.inode_limit, from<mount_tmpfs::inode_limit_t>, has_inode_limit);
    reader
        .read(mt.root_dir_mode, from<communication::client_supervisor::request::linux_namespaces::mount::mode_t>);
}

void deserialize(Reader& reader, Request::LinuxNamespaces::Mount::MountProc& mp) {
    namespace mount_proc =
        communication::client_supervisor::request::linux_namespaces::mount::mount_proc;
    mp.path = deserialize_cstring_view(reader);
    reader.read_flags(
        {
            {mp.read_only, mount_proc::flags::read_only},
            {mp.no_exec, mount_proc::flags::no_exec},
        },
        from<mount_proc::flags_t>
    );
}

void deserialize(Reader& reader, Request::LinuxNamespaces::Mount::BindMount& bm) {
    namespace bind_mount =
        communication::client_supervisor::request::linux_namespaces::mount::bind_mount;
    bm.source = deserialize_cstring_view(reader);
    bm.dest = deserialize_cstring_view(reader);
    reader.read_flags(
        {
            {bm.recursive, bind_mount::flags::recursive},
            {bm.read_only, bind_mount::flags::read_only},
            {bm.no_exec, bind_mount::flags::no_exec},
            {bm.symlink_nofollow, bind_mount::flags::symlink_nofollow},
        },
        from<bind_mount::flags_t>
    );
}

void deserialize(Reader& reader, Request::LinuxNamespaces::Mount::CreateDir& cd) {
    cd.path = deserialize_cstring_view(reader);
    reader
        .read(cd.mode, from<communication::client_supervisor::request::linux_namespaces::mount::mode_t>);
}

void deserialize(Reader& reader, Request::LinuxNamespaces::Mount::CreateFile& cf) {
    cf.path = deserialize_cstring_view(reader);
    reader
        .read(cf.mode, from<communication::client_supervisor::request::linux_namespaces::mount::mode_t>);
}

void deserialize(Reader& reader, Request::LinuxNamespaces::Mount::Operation& op) {
    using communication::client_supervisor::request::linux_namespaces::mount::OperationKind;
    using Mount = Request::LinuxNamespaces::Mount;
    switch (reader.read<OperationKind>(from<std::underlying_type_t<OperationKind>>)) {
    case OperationKind::MOUNT_TMPFS: {
        Mount::MountTmpfs mt;
        deserialize(reader, mt);
        op = mt;
    } break;
    case OperationKind::MOUNT_PROC: {
        Mount::MountProc mp;
        deserialize(reader, mp);
        op = mp;
    } break;
    case OperationKind::BIND_MOUNT: {
        Mount::BindMount bm;
        deserialize(reader, bm);
        op = bm;
    } break;
    case OperationKind::CREATE_DIR: {
        Mount::CreateDir cd;
        deserialize(reader, cd);
        op = cd;
    } break;
    case OperationKind::CREATE_FILE: {
        Mount::CreateFile cf;
        deserialize(reader, cf);
        op = cf;
    } break;
    }
}

void deserialize(Reader& reader, Request::LinuxNamespaces::Mount& m) {
    namespace mount = communication::client_supervisor::request::linux_namespaces::mount;
    m.operations.resize(reader.read<size_t>(from<mount::operations_len_t>));
    for (auto& op : m.operations) {
        deserialize(reader, op);
    }
    auto new_root_mount_path_len = reader.read<size_t>(from<mount::new_root_mount_path_len_t>);
    if (new_root_mount_path_len > 0) {
        auto csv = deserialize_cstring_view(reader);
        if (csv.size() != new_root_mount_path_len - 1) {
            THROW("invalid root_mount_path length");
        }
        m.new_root_mount_path = csv;
    } else {
        m.new_root_mount_path = std::nullopt;
    }
}

void deserialize(Reader& reader, Request::LinuxNamespaces& linux_namespaces) {
    deserialize(reader, linux_namespaces.user);
    deserialize(reader, linux_namespaces.mount);
}

void deserialize(Reader& reader, Request::Cgroup::CpuMaxBandwidth& cmb) {
    namespace cpu_max_bandwidth =
        communication::client_supervisor::request::cgroup::cpu_max_bandwidth;
    reader.read(cmb.max_usec, from<cpu_max_bandwidth::max_usec_t>);
    reader.read(cmb.period_usec, from<cpu_max_bandwidth::period_usec_t>);
}

void deserialize(Reader& reader, Request::Cgroup& cg) {
    namespace cgroup = communication::client_supervisor::request::cgroup;
    bool has_process_num_limit;
    bool has_memory_limit_in_bytes;
    bool has_swap_limit_in_bytes;
    bool has_cpu_max_bandwidth;
    reader.read_flags({
        {has_process_num_limit, cgroup::mask::process_num_limit},
        {has_memory_limit_in_bytes, cgroup::mask::memory_limit_in_bytes},
        {has_swap_limit_in_bytes, cgroup::mask::swap_limit_in_bytes},
        {has_cpu_max_bandwidth, cgroup::mask::cpu_max_bandwidth},
    }, from<cgroup::mask_t>);
    reader.read_optional_if(
        cg.process_num_limit, from<cgroup::process_num_limit_t>, has_process_num_limit
    );
    reader.read_optional_if(
        cg.memory_limit_in_bytes, from<cgroup::memory_limit_in_bytes_t>, has_memory_limit_in_bytes
    );
    reader.read_optional_if(
        cg.swap_limit_in_bytes, from<cgroup::swap_limit_in_bytes_t>, has_swap_limit_in_bytes
    );
    if (has_cpu_max_bandwidth) {
        cg.cpu_max_bandwidth.emplace();
        deserialize(reader, *cg.cpu_max_bandwidth);
    }
}

void deserialize(Reader& reader, Request::Prlimit& pr) {
    namespace prlimit = communication::client_supervisor::request::prlimit;
    bool has_max_address_space_size_in_bytes;
    bool has_max_core_file_size_in_bytes;
    bool has_cpu_time_limit_in_seconds;
    bool has_max_file_size_in_bytes;
    bool has_file_descriptors_num_limit;
    bool has_max_stack_size_in_bytes;
    reader.read_flags({
        {has_max_address_space_size_in_bytes, prlimit::mask::max_address_space_size_in_bytes},
        {has_max_core_file_size_in_bytes, prlimit::mask::max_core_file_size_in_bytes},
        {has_cpu_time_limit_in_seconds, prlimit::mask::cpu_time_limit_in_seconds},
        {has_max_file_size_in_bytes, prlimit::mask::max_file_size_in_bytes},
        {has_file_descriptors_num_limit, prlimit::mask::file_descriptors_num_limit},
        {has_max_stack_size_in_bytes, prlimit::mask::max_stack_size_in_bytes},
    }, from<prlimit::mask_t>);
    reader.read_optional_if(
        pr.max_address_space_size_in_bytes,
        from<prlimit::max_address_space_size_in_bytes_t>,
        has_max_address_space_size_in_bytes
    );
    reader.read_optional_if(
        pr.max_core_file_size_in_bytes,
        from<prlimit::max_core_file_size_in_bytes_t>,
        has_max_core_file_size_in_bytes
    );
    reader.read_optional_if(
        pr.cpu_time_limit_in_seconds,
        from<prlimit::cpu_time_limit_in_seconds_t>,
        has_cpu_time_limit_in_seconds
    );
    reader.read_optional_if(
        pr.max_file_size_in_bytes,
        from<prlimit::max_file_size_in_bytes_t>,
        has_max_file_size_in_bytes
    );
    reader.read_optional_if(
        pr.file_descriptors_num_limit,
        from<prlimit::file_descriptors_num_limit_t>,
        has_file_descriptors_num_limit
    );
    reader.read_optional_if(
        pr.max_stack_size_in_bytes,
        from<prlimit::max_stack_size_in_bytes_t>,
        has_max_stack_size_in_bytes
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
    req.result_fd = take_fd();
    req.kill_tracee_fd = take_fd();
    {
        namespace fds = request::fds;
        bool sending_executable_fd;
        bool sending_stdin_fd;
        bool sending_stdout_fd;
        bool sending_stderr_fd;
        bool sending_seccomp_bpf_fd;
        reader.read_flags({
            {sending_executable_fd, fds::mask::sending_executable_fd},
            {sending_stdin_fd, fds::mask::sending_stdin_fd},
            {sending_stdout_fd, fds::mask::sending_stdout_fd},
            {sending_stderr_fd, fds::mask::sending_stderr_fd},
            {sending_seccomp_bpf_fd, fds::mask::sending_seccomp_bpf_fd},
        }, from<fds::mask_t>);
        if (sending_executable_fd) {
            req.executable = take_fd();
        }
        req.stdin_fd = sending_stdin_fd ? optional{take_fd()} : std::nullopt;
        req.stdout_fd = sending_stdout_fd ? optional{take_fd()} : std::nullopt;
        req.stderr_fd = sending_stderr_fd ? optional{take_fd()} : std::nullopt;
        req.seccomp_bpf_fd = sending_seccomp_bpf_fd ? optional{take_fd()} : std::nullopt;
        if (!sending_executable_fd) {
            req.executable = deserialize_null_terminated_string(reader).data();
        }
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
    deserialize(reader, req.prlimit);

    {
        timespec time_limit;
        reader.read(time_limit.tv_sec, from<request::time_limit_sec_t>);
        if (time_limit.tv_sec < 0) {
            req.time_limit = std::nullopt;
        } else {
            reader.read(time_limit.tv_nsec, from<request::time_limit_nsec_t>);
            if (time_limit.tv_nsec < 0 || time_limit.tv_nsec >= 1'000'000'000) {
                THROW("invalid time_limit.nsec: ", time_limit.tv_nsec);
            }
            req.time_limit = time_limit;
        }
    }
    {
        timespec cpu_time_limit;
        reader.read(cpu_time_limit.tv_sec, from<request::cpu_time_limit_sec_t>);
        if (cpu_time_limit.tv_sec < 0) {
            req.cpu_time_limit = std::nullopt;
        } else {
            reader.read(cpu_time_limit.tv_nsec, from<request::cpu_time_limit_nsec_t>);
            if (cpu_time_limit.tv_nsec < 0 || cpu_time_limit.tv_nsec >= 1'000'000'000) {
                THROW("invalid cpu_time_limit.nsec: ", cpu_time_limit.tv_nsec);
            }
            req.cpu_time_limit = cpu_time_limit;
        }
    }
}

} // namespace sandbox::supervisor::request
