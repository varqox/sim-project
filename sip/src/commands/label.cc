#include "../sip_error.hh"
#include "../sip_package.hh"

#include <simlib/debug.hh>

namespace commands {

void label(ArgvParser args) {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    if (args.size() > 0) {
        sp.replace_variable_in_simfile("label", args.extract_next().operator StringView());
    }

    try {
        sp.simfile.load_label();
        stdlog("label = ", sp.simfile.label.value());
    } catch (const std::exception& e) {
        log_warning(e.what());
    }
}

} // namespace commands
