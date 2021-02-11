#include "simlib/debug.hh"
#include "src/sip_package.hh"

namespace commands {

void doc(ArgvParser args) {
    STACK_UNWINDING_MARK;
    SipPackage::compile_tex_files(args, false);
}

} // namespace commands
