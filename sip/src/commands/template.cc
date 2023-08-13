#include "../sip_package.hh"

#include <simlib/macros/stack_unwinding.hh>

namespace commands {

void template_command(ArgvParser args) {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    while (args.size() > 0) {
        sp.save_template(args.extract_next());
    }
}

} // namespace commands
