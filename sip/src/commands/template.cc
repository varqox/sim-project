#include "simlib/debug.hh"
#include "src/sip_package.hh"

namespace commands {

void template_command(ArgvParser args) {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    while (args.size() > 0) {
        sp.save_template(args.extract_next());
    }
}

} // namespace commands
