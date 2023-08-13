#include "../sip_error.hh"
#include "../sip_package.hh"

#include <simlib/macros/stack_unwinding.hh>

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
