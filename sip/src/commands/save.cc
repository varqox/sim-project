#include "simlib/debug.hh"
#include "src/sip_error.hh"
#include "src/sip_package.hh"

namespace commands {

void save(ArgvParser args) {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    if (args.size() == 0) {
        throw SipError("nothing to save");
    }

    while (args.size()) {
        auto arg = args.extract_next();
        if (arg == "scoring") {
            sp.rebuild_full_simfile();
            sp.save_scoring();
        } else {
            throw SipError("save: unrecognized argument ", arg);
        }
    }
}

} // namespace commands
