#include "../sip_package.hh"

#include <simlib/debug.hh>

namespace commands {

void docwatch(ArgvParser args) {
    STACK_UNWINDING_MARK;
    SipPackage::compile_tex_files(args, true);
}

} // namespace commands
