#include "../sip_package.hh"

#include <simlib/macros/stack_unwinding.hh>

namespace commands {

void doc(ArgvParser args) {
    STACK_UNWINDING_MARK;
    SipPackage::compile_tex_files(args, false);
}

} // namespace commands
