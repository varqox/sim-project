#include "../sip_package.hh"

#include <simlib/debug.hh>

namespace commands {

void genin() {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    sp.generate_test_input_files();
    sp.warn_about_tests_not_specified_as_static_or_generated();
}

} // namespace commands
