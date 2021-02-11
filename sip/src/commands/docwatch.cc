#include "simlib/debug.hh"
#include "src/sip_package.hh"

namespace commands {

void docwatch(ArgvParser args) {
    STACK_UNWINDING_MARK;
    SipPackage::compile_tex_files(args, true);
}

} // namespace commands
