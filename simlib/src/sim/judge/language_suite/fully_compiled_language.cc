#include <algorithm>
#include <chrono>
#include <fcntl.h>
#include <simlib/errmsg.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/file_info.hh>
#include <simlib/file_path.hh>
#include <simlib/macros/throw.hh>
#include <simlib/merge.hh>
#include <simlib/overloaded.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/sandbox/seccomp/allow_common_safe_syscalls.hh>
#include <simlib/sandbox/seccomp/bpf_builder.hh>
#include <simlib/sim/judge/language_suite/fully_compiled_language.hh>
#include <simlib/slice.hh>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <variant>
#include <vector>

using MountTmpfs = sandbox::RequestOptions::LinuxNamespaces::Mount::MountTmpfs;
using BindMount = sandbox::RequestOptions::LinuxNamespaces::Mount::BindMount;
using CreateDir = sandbox::RequestOptions::LinuxNamespaces::Mount::CreateDir;
using CreateFile = sandbox::RequestOptions::LinuxNamespaces::Mount::CreateFile;

namespace sim::judge::language_suite {

FullyCompiledLanguage::FullyCompiledLanguage(
    std::string compiler_executable_path, FileDescriptor compiler_seccomp_bpf_fd
)
: compiler_executable_path{std::move(compiler_executable_path)}
, compiler_seccomp_bpf_fd{std::move(compiler_seccomp_bpf_fd)}
, executable_seccomp_bpf_fd{[] {
    auto bpf = sandbox::seccomp::BpfBuilder{};
    sandbox::seccomp::allow_common_safe_syscalls(bpf);
    return bpf.export_to_fd();
}()} {}

bool FullyCompiledLanguage::is_supported() {
    if (access(compiler_executable_path, F_OK) != 0) {
        return false;
    }
    return std::visit(
        overloaded{
            [](const sandbox::result::Ok& res_ok) {
                return res_ok.si == sandbox::Si{.code = CLD_EXITED, .status = 0};
            },
            [](const sandbox::result::Error& /*res_err*/) { return false; },
        },
        is_supported_impl({
            .time_limit = std::chrono::seconds{1},
            .cpu_time_limit = std::chrono::seconds{1},
            .memory_limit_in_bytes = 1 << 30,
            .max_file_size_in_bytes = 0,
        })
    );
}

Result<void, FileDescriptor>
FullyCompiledLanguage::compile(FilePath source, CompileOptions options) {
    executable_file_is_ready = false;

    if (options.cache &&
        options.cache->compilation_cache.copy_from_cache_if_newer_than(
            options.cache->cached_name,
            executable_tmp_file.path(),
            std::max(get_modification_time(source), get_modification_time(compiler_executable_path))
        ))
    {
        executable_file_is_ready = true;
        return Ok{};
    }

    auto compilation_errors_fd = FileDescriptor{memfd_create("compilation errors fd", MFD_CLOEXEC)};
    if (!compilation_errors_fd.is_open()) {
        THROW("memfd_create()", errmsg());
    }

    auto cache = options.cache;
    auto sres =
        compile_impl(source, executable_tmp_file.path(), compilation_errors_fd, std::move(options));
    return std::visit(
        overloaded{
            [&](const sandbox::result::Ok& ok) -> Result<void, FileDescriptor> {
                if (ok.si == sandbox::Si{.code = CLD_EXITED, .status = 0}) {
                    executable_file_is_ready = true;
                    if (cache) {
                        cache->compilation_cache.save_or_override(
                            cache->cached_name, executable_tmp_file.path()
                        );
                    }
                    return Ok{};
                }
                return Err{std::move(compilation_errors_fd)};
            },
            [](const sandbox::result::Error& err) -> Result<void, FileDescriptor> {
                THROW(err.description);
            },
        },
        sres
    );
}

Suite::RunHandle FullyCompiledLanguage::async_run(
    Slice<std::string_view> args,
    RunOptions options,
    Slice<sandbox::RequestOptions::LinuxNamespaces::Mount::Operation> mount_ops
) {
    if (!executable_file_is_ready) {
        THROW("cannot run without successful compilation preceding the run");
    }
    return RunHandle{sc.send_request(
        "/exe",
        merge(std::vector<std::string_view>{""}, args),
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
                                        .inode_limit = 6 + options.rootfs.inode_limit,
                                        .read_only = false,
                                    },
                                    CreateDir{.path = "/../lib"},
                                    CreateDir{.path = "/../lib64"},
                                    CreateDir{.path = "/../usr"},
                                    CreateDir{.path = "/../usr/lib"},
                                    CreateDir{.path = "/../usr/lib64"},
                                    CreateFile{.path = "/../exe"},
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
                                        .source = "/usr/lib64",
                                        .dest = "/../usr/lib64",
                                        .no_exec = false,
                                    },
                                    BindMount{
                                        .source = executable_tmp_file.path(),
                                        .dest = "/../exe",
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
            .seccomp_bpf_fd = executable_seccomp_bpf_fd,
        }
    )};
}

} // namespace sim::judge::language_suite
