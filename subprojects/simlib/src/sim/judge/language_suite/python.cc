#include <cerrno>
#include <chrono>
#include <simlib/macros/throw.hh>
#include <simlib/merge.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/sandbox/seccomp/allow_common_safe_syscalls.hh>
#include <simlib/sandbox/seccomp/bpf_builder.hh>
#include <simlib/sim/judge/language_suite/fully_interpreted_language.hh>
#include <simlib/sim/judge/language_suite/python.hh>
#include <simlib/slice.hh>
#include <string_view>
#include <sys/ioctl.h>
#include <vector>

using MountTmpfs = sandbox::RequestOptions::LinuxNamespaces::Mount::MountTmpfs;
using BindMount = sandbox::RequestOptions::LinuxNamespaces::Mount::BindMount;
using CreateDir = sandbox::RequestOptions::LinuxNamespaces::Mount::CreateDir;
using CreateFile = sandbox::RequestOptions::LinuxNamespaces::Mount::CreateFile;

namespace sim::judge::language_suite {

Python::Python()
: FullyInterpretedLanguage{"/usr/bin/python3", [] {
                               auto bpf = sandbox::seccomp::BpfBuilder{SCMP_ACT_ERRNO(ENOSYS)};
                               sandbox::seccomp::allow_common_safe_syscalls(bpf);
                               bpf.allow_syscall(
                                   SCMP_SYS(ioctl), sandbox::seccomp::ARG1_EQ{FIOCLEX}
                               );
                               return bpf.export_to_fd();
                           }()} {}

Suite::PreparedProgram Python::prepared_program() {
    if (!source_file_is_ready) {
        THROW("cannot run without successful compilation preceding the run");
    }
    return prepare_program(
        source_tmp_file.path(), seccomp_bpf_fd, interpreter_executable_path, run
    );
}

Suite::RunHandle Python::async_run(
    Slice<std::string_view> args,
    RunOptions options,
    Slice<sandbox::RequestOptions::LinuxNamespaces::Mount::Operation> mount_ops
) {
    return run(
        sc,
        source_tmp_file.path(),
        seccomp_bpf_fd,
        interpreter_executable_path,
        args,
        std::move(options),
        mount_ops
    );
}

Suite::RunHandle Python::run(
    sandbox::SupervisorConnection& sc,
    FilePath artifact,
    int seccomp_bpf_fd,
    std::string_view interpreter,
    Slice<std::string_view> args,
    RunOptions options,
    Slice<sandbox::RequestOptions::LinuxNamespaces::Mount::Operation> mount_ops
) {
    return RunHandle{sc.send_request(
        interpreter,
        merge(std::vector<std::string_view>{"python3", "source.py"}, args),
        {
            .stdin_fd = options.stdin_fd,
            .stdout_fd = options.stdout_fd,
            .stderr_fd = options.stderr_fd,
            .env = std::move(options.env),
            .linux_namespaces =
                {
                    .user =
                        {
                            .inside_uid = 1000,
                            .inside_gid = 1000,
                        },
                    .mount =
                        {
                            .operations = merge(
                                std::vector<
                                    sandbox::RequestOptions::LinuxNamespaces::Mount::Operation>{
                                    MountTmpfs{
                                        .path = "/",
                                        .max_total_size_of_files_in_bytes =
                                            options.rootfs.max_total_size_of_files_in_bytes,
                                        .inode_limit = 7 + options.rootfs.inode_limit,
                                        .read_only = false,
                                    },
                                    CreateDir{.path = "/../lib"},
                                    CreateDir{.path = "/../lib64"},
                                    CreateDir{.path = "/../usr"},
                                    CreateDir{.path = "/../usr/bin"},
                                    CreateDir{.path = "/../usr/lib"},
                                    CreateDir{.path = "/../usr/lib64"},
                                    CreateFile{.path = "/../source.py"},
                                    BindMount{
                                        .source = "/lib",
                                        .dest = "/../lib",
                                        .no_exec = false,
                                    },
                                    BindMount{
                                        .source = "/lib64",
                                        .dest = "/../lib64",
                                        .no_exec = false,
                                    },
                                    BindMount{
                                        .source = "/usr/bin",
                                        .dest = "/../usr/bin",
                                        .no_exec = false,
                                    },
                                    BindMount{
                                        .source = "/usr/lib",
                                        .dest = "/../usr/lib",
                                        .no_exec = false,
                                    },
                                    BindMount{
                                        .source = "/usr/lib64",
                                        .dest = "/../usr/lib64",
                                        .no_exec = false,
                                    },
                                    BindMount{
                                        .source = {artifact.data(), artifact.size()},
                                        .dest = "/../source.py",
                                        .no_exec = false,
                                    },
                                },
                                mount_ops
                            ),
                            .new_root_mount_path = "/..",
                        },
                },
            .cgroup =
                {
                    .process_num_limit = options.process_num_limit,
                    .memory_limit_in_bytes = options.memory_limit_in_bytes,
                    .swap_limit_in_bytes = 0,
                },
            .prlimit =
                {
                    .max_core_file_size_in_bytes = 0,
                    .cpu_time_limit_in_seconds =
                        std::chrono::ceil<std::chrono::seconds>(
                            options.cpu_time_limit + std::chrono::milliseconds{100}
                        )
                            .count(),
                    .max_file_size_in_bytes = options.max_file_size_in_bytes,
                    .max_stack_size_in_bytes = options.max_stack_size_in_bytes,
                },
            .time_limit = options.time_limit,
            .cpu_time_limit = options.cpu_time_limit,
            .seccomp_bpf_fd = seccomp_bpf_fd,
            .started_fd = options.started_fd,
        }
    )};
}

} // namespace sim::judge::language_suite
