#include <cerrno>
#include <optional>
#include <simlib/file_info.hh>
#include <simlib/file_path.hh>
#include <simlib/merge.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/sandbox/seccomp/allow_common_safe_syscalls.hh>
#include <simlib/sandbox/seccomp/bpf_builder.hh>
#include <simlib/sim/judge/language_suite/fully_compiled_language.hh>
#include <simlib/sim/judge/language_suite/rust.hh>
#include <simlib/slice.hh>
#include <string_view>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <vector>

using MountTmpfs = sandbox::RequestOptions::LinuxNamespaces::Mount::MountTmpfs;
using BindMount = sandbox::RequestOptions::LinuxNamespaces::Mount::BindMount;
using CreateDir = sandbox::RequestOptions::LinuxNamespaces::Mount::CreateDir;
using CreateFile = sandbox::RequestOptions::LinuxNamespaces::Mount::CreateFile;
using MountProc = sandbox::RequestOptions::LinuxNamespaces::Mount::MountProc;

namespace sim::judge::language_suite {

Rust::Rust(Edition edition)
: FullyCompiledLanguage{"/usr/bin/rustc", [] {
    auto bpf = sandbox::seccomp::BpfBuilder{SCMP_ACT_ERRNO(ENOSYS)};
    sandbox::seccomp::allow_common_safe_syscalls(bpf);
    bpf.allow_syscall(SCMP_SYS(ioctl), sandbox::seccomp::ARG1_EQ{FIONBIO});
    return bpf.export_to_fd();
}()}
, edition_str([&] {
    switch (edition) {
    case Edition::ed2018: return "2018";
    case Edition::ed2021: return "2021";
    case Edition::ed2024: return "2024";
    }
    __builtin_unreachable();
}()) {}

sandbox::Result Rust::run_compiler(
    Slice<std::string_view> extra_args,
    std::optional<int> compilation_errors_fd,
    Slice<sandbox::RequestOptions::LinuxNamespaces::Mount::Operation> mount_ops,
    CompileOptions options
) {
    return sc
        .await_result(
            sc
                .send_request(
                    compiler_executable_path,
                    merge(
                        std::vector<std::string_view>{
                            "rustc",
                            "--edition",
                            edition_str,
                            "--codegen",
                            "debuginfo=0",
                            "--codegen",
                            "opt-level=2",
                            "--codegen",
                            "panic=abort",
                        },
                        extra_args
                    ),
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
                                                merge(
                                                    std::
                                                        vector<
                                                            sandbox::RequestOptions::
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
                                                            CreateDir{.path = "/../proc"},
                                                            CreateDir{.path = "/../tmp"},
                                                            CreateDir{.path = "/../usr"},
                                                            CreateDir{.path = "/../usr/bin"},
                                                            CreateDir{.path = "/../usr/lib"},
                                                            CreateDir{.path = "/../usr/lib64"},
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
                                                            MountProc{
                                                                .path = "/../proc",
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
                                                                .source = "/usr/bin/rustc",
                                                                .dest = "/../proc/self/exe",
                                                                .no_exec = false,
                                                            },
                                                        },
                                                    path_exists("/etc/alternatives/") ? Slice<
                                                                                            sandbox::RequestOptions::LinuxNamespaces::Mount::Operation>{{
                                                                                            CreateDir{
                                                                                                .path = "/../etc"},
                                                                                            CreateDir{
                                                                                                .path = "/../etc/alternatives"},
                                                                                            BindMount{
                                                                                                .source =
                                                                                                    "/etc/alternatives/",
                                                                                                .dest = "/../etc/alternatives",
                                                                                            },
                                                                                        }}
                                                                                      : Slice<sandbox::
                                                                                                  RequestOptions::
                                                                                                      LinuxNamespaces::
                                                                                                          Mount::Operation>{}
                                                ),
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

sandbox::Result Rust::is_supported_impl(CompileOptions options) {
    return run_compiler({{"--print", "sysroot"}}, std::nullopt, {}, std::move(options));
}

sandbox::Result Rust::compile_impl(
    FilePath source, FilePath executable, int compilation_errors_fd, CompileOptions options
) {
    return run_compiler(
        {{"source.rs", "-o", "exe"}},
        compilation_errors_fd,
        {{
            CreateFile{.path = "/../exe"},
            CreateFile{.path = "/../source.rs"},
            BindMount{
                .source = std::string_view{executable},
                .dest = "/../exe",
                .read_only = false,
            },
            BindMount{
                .source = std::string_view{source},
                .dest = "/../source.rs",
            },
        }},
        std::move(options)
    );
}

} // namespace sim::judge::language_suite
