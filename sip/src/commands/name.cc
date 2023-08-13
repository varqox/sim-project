#include "../sip_error.hh"
#include "../sip_package.hh"

#include <simlib/macros/stack_unwinding.hh>

namespace commands {

void name(ArgvParser args) {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    if (args.size() > 0) {
        sp.replace_variable_in_simfile("name", args.extract_next().operator StringView());
    }

    try {
        sp.simfile.load_name();
        stdlog("name = ", sp.simfile.name.value());
    } catch (const std::exception& e) {
        log_warning(e.what());
    }
}

} // namespace commands
