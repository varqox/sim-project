#include "../sip_error.hh"
#include "../sip_package.hh"

#include <simlib/macros/stack_unwinding.hh>

namespace commands {

void mem(ArgvParser args) {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    if (args.size() > 0) {
        auto new_mem_limit = args.extract_next();
        if (not is_digit_not_less_than<1>(new_mem_limit)) {
            throw SipError("memory limit has to be a positive integer");
        }

        sp.replace_variable_in_simfile("memory_limit", new_mem_limit);
    }

    sp.simfile.load_global_memory_limit_only();
    if (not sp.simfile.global_mem_limit.has_value()) {
        stdlog("mem is not set");
    } else {
        stdlog("mem = ", sp.simfile.global_mem_limit.value() >> 20, " MiB");
    }
}

} // namespace commands
