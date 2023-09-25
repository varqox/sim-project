#pragma once

#include <simlib/file_descriptor.hh>
#include <simlib/file_path.hh>
#include <simlib/result.hh>
#include <simlib/sim/judge/language_suite/suite.hh>
#include <simlib/slice.hh>
#include <simlib/temporary_file.hh>

namespace sim::judge::language_suite {

class FullyInterpretedLanguage : public Suite {
protected:
    FileDescriptor interpreter_executable_fd;
    TemporaryFile source_tmp_file{"/tmp/sim_fully_interpreted_language_suite_source.XXXXXX"};
    FileDescriptor seccomp_bpf_fd;

public:
    explicit FullyInterpretedLanguage(
        FilePath interpreter_executable_path, FileDescriptor seccomp_bpf_fd
    );

    [[nodiscard]] bool is_supported() final { return interpreter_executable_fd.is_open(); }

    Result<void, FileDescriptor> compile(FilePath source, CompileOptions /*options*/) final;

    RunHandle async_run(
        Slice<std::string_view> args,
        RunOptions options,
        Slice<sandbox::RequestOptions::LinuxNamespaces::Mount::Operation> mount_ops
    ) override = 0;
};

} // namespace sim::judge::language_suite
