#include "simlib/debug.hh"
#include "src/sip_package.hh"

namespace commands {

void clean() {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    sp.clean();
}

} // namespace commands
