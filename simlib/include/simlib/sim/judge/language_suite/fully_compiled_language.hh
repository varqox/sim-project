#pragma once

#include <simlib/file_descriptor.hh>
#include <simlib/file_path.hh>
#include <simlib/result.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/sim/judge/language_suite/suite.hh>
#include <simlib/slice.hh>
#include <simlib/temporary_file.hh>
#include <string_view>

namespace sim::judge::language_suite {

class FullyCompiledLanguage : public Suite {
protected:
    FileDescriptor compiler_executable_fd;
    FileDescriptor compiler_seccomp_bpf_fd;

private:
    TemporaryFile executable_tmp_file{"/tmp/sim_fully_compiled_language_suite_executable.XXXXXX"};
    FileDescriptor executable_file_fd;

protected:
    FileDescriptor executable_seccomp_bpf_fd;

public:
    explicit FullyCompiledLanguage(
        FilePath compiler_executable_path, FileDescriptor compiler_seccomp_bpf_fd
    );

    [[nodiscard]] bool is_supported() final;

    Result<void, FileDescriptor> compile(FilePath source, CompileOptions options) final;

    RunHandle async_run(
        Slice<std::string_view> args,
        RunOptions options,
        Slice<sandbox::RequestOptions::LinuxNamespaces::Mount::Operation> mount_ops
    ) final;

protected:
    [[nodiscard]] virtual sandbox::Result is_supported_impl(CompileOptions options) = 0;

    virtual sandbox::Result compile_impl(
        FilePath source, FilePath executable, int compilation_errors_fd, CompileOptions options
    ) = 0;
};

} // namespace sim::judge::language_suite
