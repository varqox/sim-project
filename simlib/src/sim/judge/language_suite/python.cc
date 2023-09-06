#include <cerrno>
#include <chrono>
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
: FullyInterpretedLanguage{
      "/usr/bin/python", [] {
          auto bpf = sandbox::seccomp::BpfBuilder{};
          sandbox::seccomp::allow_common_safe_syscalls(bpf);
          bpf.allow_syscall(SCMP_SYS(ioctl), sandbox::seccomp::ARG1_EQ{FIOCLEX});
          bpf.err_syscall(EPERM, SCMP_SYS(sysinfo));
          return bpf.export_to_fd();
      }()} {}

Suite::RunHandle Python::async_run(
    Slice<std::string_view> args,
    RunOptions options,
    Slice<sandbox::RequestOptions::LinuxNamespaces::Mount::Operation> mount_ops
) {
    return RunHandle{sc.send_request(
        interpreter_executable_fd,
        merge(std::vector<std::string_view>{"python", "source.py"}, args),
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
                                        .inode_limit = 5 + options.rootfs.inode_limit,
                                        .read_only = false,
                                    },
                                    CreateDir{.path = "/../lib"},
                                    CreateDir{.path = "/../lib64"},
                                    CreateDir{.path = "/../usr"},
                                    CreateDir{.path = "/../usr/lib"},
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
                                        .source = "/usr/lib",
                                        .dest = "/../usr/lib",
                                        .no_exec = false,
                                    },
                                    BindMount{
                                        .source = source_tmp_file.path(),
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
        }
    )};
}

} // namespace sim::judge::language_suite
