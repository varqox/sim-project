#include "../sip_package.hh"

#include <simlib/debug.hh>

namespace commands {

void unset(ArgvParser args) {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    while (args.size() > 0) {
        sp.replace_variable_in_simfile(args.extract_next(), std::nullopt);
    }
}

} // namespace commands
