#include <cstring>
#include <simlib/concat_tostr.hh>
#include <simlib/sandbox/si.hh>
#include <sys/wait.h>

namespace sandbox {

[[nodiscard]] std::string Si::description() const {
    auto signal_description = [](const char* prefix, int signum) {
        auto abbrev = sigabbrev_np(signum);
        auto desc = sigdescr_np(signum);
        if (abbrev) {
            if (desc) {
                return concat_tostr(prefix, ' ', abbrev, " - ", desc);
            }
            return concat_tostr(prefix, ' ', abbrev);
        }
        if (desc) {
            return concat_tostr(prefix, " with number ", signum, " - ", desc);
        }
        return concat_tostr(prefix, " with number ", signum);
    };
    switch (code) {
    case CLD_EXITED: return concat_tostr("exited with ", status);
    case CLD_KILLED: return signal_description("killed by signal", status);
    case CLD_DUMPED: return signal_description("killed and dumped by signal", status);
    case CLD_TRAPPED: return signal_description("trapped by signal", status);
    case CLD_STOPPED: return signal_description("stopped by signal", status);
    case CLD_CONTINUED: return signal_description("continued by signal", status);
    }
    return concat_tostr("unable to describe (code ", code, ", status ", status, ')');
}

} // namespace sandbox
