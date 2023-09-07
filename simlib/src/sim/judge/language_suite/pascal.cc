#include <optional>
#include <simlib/file_path.hh>
#include <simlib/merge.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/sandbox/seccomp/bpf_builder.hh>
#include <simlib/sim/judge/language_suite/fully_compiled_language.hh>
#include <simlib/sim/judge/language_suite/pascal.hh>
#include <simlib/slice.hh>
#include <string_view>
#include <sys/ioctl.h>
#include <vector>

using MountTmpfs = sandbox::RequestOptions::LinuxNamespaces::Mount::MountTmpfs;
using BindMount = sandbox::RequestOptions::LinuxNamespaces::Mount::BindMount;
using CreateDir = sandbox::RequestOptions::LinuxNamespaces::Mount::CreateDir;
using CreateFile = sandbox::RequestOptions::LinuxNamespaces::Mount::CreateFile;

namespace sim::judge::language_suite {

Pascal::Pascal()
: FullyCompiledLanguage{"/usr/bin/fpc", [] {
                            auto bpf = sandbox::seccomp::BpfBuilder{};
                            bpf.allow_syscall(SCMP_SYS(access));
                            bpf.allow_syscall(SCMP_SYS(arch_prctl));
                            bpf.allow_syscall(SCMP_SYS(brk));
                            bpf.allow_syscall(SCMP_SYS(chmod));
                            bpf.allow_syscall(SCMP_SYS(close));
                            bpf.allow_syscall(SCMP_SYS(execve));
                            bpf.allow_syscall(SCMP_SYS(execveat));
                            bpf.allow_syscall(SCMP_SYS(exit_group));
                            bpf.allow_syscall(SCMP_SYS(fcntl));
                            bpf.allow_syscall(SCMP_SYS(fork));
                            bpf.allow_syscall(SCMP_SYS(fstat));
                            bpf.allow_syscall(SCMP_SYS(getcwd));
                            bpf.allow_syscall(SCMP_SYS(getdents64));
                            bpf.allow_syscall(SCMP_SYS(getpid));
                            bpf.allow_syscall(SCMP_SYS(getrandom));
                            bpf.allow_syscall(SCMP_SYS(getrlimit));
                            bpf.allow_syscall(SCMP_SYS(getrusage));
                            bpf.allow_syscall(SCMP_SYS(gettimeofday));
                            bpf.allow_syscall(SCMP_SYS(ioctl), sandbox::seccomp::ARG1_EQ{TCGETS});
                            bpf.allow_syscall(SCMP_SYS(kill));
                            bpf.allow_syscall(SCMP_SYS(lseek));
                            bpf.allow_syscall(SCMP_SYS(mmap));
                            bpf.allow_syscall(SCMP_SYS(mprotect));
                            bpf.allow_syscall(SCMP_SYS(munmap));
                            bpf.allow_syscall(SCMP_SYS(newfstatat));
                            bpf.allow_syscall(SCMP_SYS(open));
                            bpf.allow_syscall(SCMP_SYS(openat));
                            bpf.allow_syscall(SCMP_SYS(pread64));
                            bpf.allow_syscall(SCMP_SYS(prlimit64));
                            bpf.allow_syscall(SCMP_SYS(read));
                            bpf.allow_syscall(SCMP_SYS(readlink));
                            bpf.allow_syscall(SCMP_SYS(rseq));
                            bpf.allow_syscall(SCMP_SYS(rt_sigaction));
                            bpf.allow_syscall(SCMP_SYS(sched_yield));
                            bpf.allow_syscall(SCMP_SYS(set_robust_list));
                            bpf.allow_syscall(SCMP_SYS(set_tid_address));
                            bpf.allow_syscall(SCMP_SYS(stat));
                            bpf.allow_syscall(SCMP_SYS(time));
                            bpf.allow_syscall(SCMP_SYS(umask));
                            bpf.allow_syscall(SCMP_SYS(unlink));
                            bpf.allow_syscall(SCMP_SYS(wait4));
                            bpf.allow_syscall(SCMP_SYS(write));
                            bpf.err_syscall(EPERM, SCMP_SYS(sysinfo));
                            return bpf.export_to_fd();
                        }()} {}

sandbox::Result Pascal::run_compiler(
    Slice<std::string_view> extra_args,
    std::optional<int> compilation_errors_fd,
    Slice<sandbox::RequestOptions::LinuxNamespaces::Mount::Operation> mount_ops,
    CompileOptions options
) {
    return sc
        .await_result(
            sc
                .send_request(
                    compiler_executable_fd,
                    merge(std::vector<std::string_view>{"fpc", "-O2", "-XS", "-Xt"}, extra_args),
                    {
                        .stdout_fd = compilation_errors_fd,
                        .stderr_fd = compilation_errors_fd,
                        .env = {{"PATH=/usr/bin"}},
                        .linux_namespaces =
                            {
                                .user =
                                    {
                                        .inside_uid = 1000,
                                        .inside_gid = 1000,
                                    },
                                .mount =
                                    {
                                        .operations =
                                            merge(
                                                std::vector<sandbox::RequestOptions::
                                                                LinuxNamespaces::Mount::Operation>{
                                                    MountTmpfs{
                                                        .path = "/",
                                                        .max_total_size_of_files_in_bytes =
                                                            options.max_file_size_in_bytes,
                                                        .inode_limit = 32,
                                                        .read_only = false,
                                                    },
                                                    CreateDir{.path = "/../lib"},
                                                    CreateDir{.path = "/../lib64"},
                                                    CreateDir{.path = "/../tmp"},
                                                    CreateDir{.path = "/../usr"},
                                                    CreateDir{.path = "/../usr/bin"},
                                                    CreateDir{.path = "/../usr/lib"},
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
                                                },
                                                mount_ops
                                            ),
                                        .new_root_mount_path = "/..",
                                    },
                            },
                        .cgroup =
                            {
                                .process_num_limit = 32,
                                .memory_limit_in_bytes = options.memory_limit_in_bytes,
                                .cpu_max_bandwidth =
                                    sandbox::RequestOptions::Cgroup::CpuMaxBandwidth{
                                        .max_usec = 10000,
                                        .period_usec = 10000,
                                    },
                            },
                        .prlimit =
                            {
                                .max_core_file_size_in_bytes = 0,
                                .max_file_size_in_bytes = options.max_file_size_in_bytes,
                            },
                        .time_limit = options.time_limit,
                        .cpu_time_limit = options.cpu_time_limit,
                        .seccomp_bpf_fd = compiler_seccomp_bpf_fd,
                    }
                )
        );
}

sandbox::Result Pascal::is_supported_impl(CompileOptions options) {
    return run_compiler({{"-iV"}}, std::nullopt, {}, std::move(options));
}

sandbox::Result Pascal::compile_impl(
    FilePath source, FilePath executable, int compilation_errors_fd, CompileOptions options
) {
    return run_compiler(
        {{"source.pas", "-oexe"}},
        compilation_errors_fd,
        {{
            CreateFile{.path = "/../exe"},
            CreateFile{.path = "/../source.pas"},
            BindMount{
                .source = std::string_view{executable},
                .dest = "/../exe",
                .read_only = false,
            },
            BindMount{
                .source = std::string_view{source},
                .dest = "/../source.pas",
            },
        }},
        std::move(options)
    );
}

} // namespace sim::judge::language_suite
