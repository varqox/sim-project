#include "simlib/debug.hh"
#include "src/sip_package.hh"

namespace commands {

void regenin() {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    sp.remove_test_files_not_specified_in_sipfile();
    sp.generate_test_input_files();
    sp.warn_about_tests_not_specified_as_static_or_generated();
}

} // namespace commands
