#include "../sip_package.hh"

#include <simlib/macros/stack_unwinding.hh>

namespace commands {

void regenin() {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    sp.remove_test_files_not_specified_in_sipfile();
    sp.generate_test_input_files();
    sp.warn_about_tests_not_specified_as_static_or_generated();
}

} // namespace commands
