#include "../sip_package.hh"

#include <simlib/debug.hh>

namespace commands {

void clean() {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    sp.clean();
}

} // namespace commands
