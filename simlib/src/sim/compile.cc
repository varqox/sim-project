#include "simlib/sim/compile.hh"
#include "simlib/spawner.hh"
#include "simlib/unlinked_temporary_file.hh"

using std::string;
using std::vector;

namespace sim {

int compile(StringView dir_to_chdir, vector<string> compile_command,
        std::optional<std::chrono::nanoseconds> time_limit, string* c_errors,
        size_t c_errors_max_len, const string& proot_path) {
    using std::chrono_literals::operator""ns;

    if (time_limit.has_value() and time_limit.value() <= 0ns) {
        THROW("If set, time_limit has to be greater than 0");
    }

    FileDescriptor cef;
    if (c_errors) {
        cef = open_unlinked_tmp_file(O_APPEND | O_CLOEXEC);
        if (not cef.is_open()) {
            THROW("Failed to open 'compile_errors'", errmsg());
        }
    }

    /*
     * Compiler is PRooted to make compilation safer (e.g. prevents including
     * unwanted files)
     */
    vector<string> args = (proot_path.empty()
                    ? std::initializer_list<string>{}
                    : std::initializer_list<string>{// clang-format off
                proot_path,
                "-v", "-1",
                "-r", dir_to_chdir.to_string(),
                "-b", "/usr",
                "-b", "/bin",
                "-b", "/lib",
                "-b", "/lib32",
                "-b", "/libx32",
                "-b", "/lib64",
                "-b", "/etc/alternatives/",
                "-b", "/etc/fpc.cfg"}); // TODO: make this a specific option for the FPC compiler
    // clang-format on

    args.insert(args.end(), compile_command.begin(), compile_command.end());

    // Run the compiler
    Spawner::ExitStat es = Spawner::run(args[0], args,
            {-1, cef, cef, time_limit, 1 << 30 /* 1 GiB */, {},
                    (proot_path.empty() ? intentional_unsafe_cstring_view(concat(dir_to_chdir))
                                        : ".")});

    // Check for errors
    if (es.si.code != CLD_EXITED or es.si.status != 0) {
        if (c_errors) {
            *c_errors = (time_limit.has_value() and es.runtime >= time_limit.value()
                            ? "Compilation time limit exceeded"
                            : get_file_contents(cef, 0, c_errors_max_len));
        }

        return 2;
    }

    if (c_errors) {
        *c_errors = "";
    }

    return 0;
}

} // namespace sim
