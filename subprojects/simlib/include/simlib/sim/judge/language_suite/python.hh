#pragma once

#include <simlib/sandbox/sandbox.hh>
#include <simlib/sim/judge/language_suite/fully_interpreted_language.hh>
#include <simlib/slice.hh>
#include <string_view>

namespace sim::judge::language_suite {

class Python final : public FullyInterpretedLanguage {
public:
    Python();

    [[nodiscard]] PreparedProgram prepared_program() final;

    RunHandle async_run(
        Slice<std::string_view> args,
        RunOptions options,
        Slice<sandbox::RequestOptions::LinuxNamespaces::Mount::Operation> mount_ops
    ) final;

private:
    static RunHandle
    run(sandbox::SupervisorConnection& sc,
        FilePath artifact,
        int seccomp_bpf_fd,
        std::string_view interpreter,
        Slice<std::string_view> args,
        RunOptions options,
        Slice<sandbox::RequestOptions::LinuxNamespaces::Mount::Operation> mount_ops);
};

} // namespace sim::judge::language_suite
