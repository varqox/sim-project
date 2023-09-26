#include <simlib/errmsg.hh>
#include <simlib/file_manip.hh>
#include <simlib/file_path.hh>
#include <simlib/macros/throw.hh>
#include <simlib/sim/judge/language_suite/fully_interpreted_language.hh>
#include <utility>

namespace sim::judge::language_suite {

FullyInterpretedLanguage::FullyInterpretedLanguage(
    std::string interpreter_executable_path, FileDescriptor seccomp_bpf_fd
)
: interpreter_executable_path{std::move(interpreter_executable_path)}
, seccomp_bpf_fd{std::move(seccomp_bpf_fd)} {}

Result<void, FileDescriptor>
FullyInterpretedLanguage::compile(FilePath source, CompileOptions /*options*/) {
    if (copy(source, source_tmp_file.path())) {
        THROW("copy()", errmsg());
    }
    return Ok{};
}

} // namespace sim::judge::language_suite
