#include "../sip_package.hh"

#include <simlib/macros/stack_unwinding.hh>

namespace commands {

void regen(std::optional<sim::ExecutionFormat> format) {
    STACK_UNWINDING_MARK;

    SipPackage sp(format);
    sp.remove_test_files_not_specified_in_sipfile();
    sp.generate_test_input_files();

    sp.simfile.load_interactive();
    if (not sp.simfile.interactive) {
        sp.generate_test_output_files();
    }

    if (sp.simfile.communication) {
        sp.rebuild_full_simfile();
    }
    if (access("Simfile", F_OK) == 0) {
        sp.save_limits();
    }
    sp.warn_about_tests_not_specified_as_static_or_generated();
}

} // namespace commands
