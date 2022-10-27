#include "../sip_error.hh"
#include "../sip_package.hh"

#include <simlib/debug.hh>
#include <simlib/file_info.hh>

namespace commands {

void checker(ArgvParser args) {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    if (args.size() > 0) {
        auto checker = args.extract_next();
        if (not checker.empty() and access(checker, F_OK) != 0) {
            throw SipError("checker: ", checker, " does not exist");
        }

        sp.replace_variable_in_simfile("checker", checker);
    }

    sp.simfile.load_checker();
    stdlog("checker = ", sp.simfile.checker.value_or(""));
}

} // namespace commands
