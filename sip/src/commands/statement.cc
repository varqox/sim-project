#include "simlib/debug.hh"
#include "simlib/file_info.hh"
#include "src/sip_error.hh"
#include "src/sip_package.hh"

namespace commands {

void statement(ArgvParser args) {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    if (args.size() > 0) {
        auto statement = args.extract_next();
        if (not statement.empty() and access(statement, F_OK) != 0) {
            throw SipError("statement: ", statement, " does not exist");
        }

        sp.replace_variable_in_simfile("statement", statement);
    }

    try {
        sp.simfile.load_statement();
        stdlog("statement = ", sp.simfile.statement.value());
    } catch (const std::exception& e) {
        log_warning(e.what());
    }
}

} // namespace commands
