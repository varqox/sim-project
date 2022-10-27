#include "../sip_error.hh"
#include "../sip_package.hh"

#include <simlib/debug.hh>

namespace commands {

void interactive(ArgvParser args) {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    if (args.size() > 0) {
        auto new_interactive = args.extract_next();
        if (not is_one_of(new_interactive, "true", "false")) {
            throw SipError("interactive should have a value of true or false");
        }

        sp.replace_variable_in_simfile("interactive", new_interactive);
    }

    sp.simfile.load_interactive();
    stdlog("interactive = ", sp.simfile.interactive);
}

} // namespace commands
