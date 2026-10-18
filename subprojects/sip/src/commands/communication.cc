#include "../sip_error.hh"
#include "../sip_package.hh"
#include "../templates/templates.hh"
#include "commands.hh"

#include <simlib/file_contents.hh>
#include <simlib/file_info.hh>
#include <simlib/macros/stack_unwinding.hh>

namespace commands {

void communication(ArgvParser args) {
    STACK_UNWINDING_MARK;

    if (args.size() == 0) {
        throw SipError("missing communication command - see -h for help");
    }
    auto command = args.extract_next();
    if (command == "init") {
        if (args.size() == 0) {
            throw SipError("missing directory argument - see -h for help");
        }
        auto init_args = args;
        auto dir = args.extract_next();
        if (path_exists(concat_tostr(dir, "/manager/manager.cc"))) {
            throw SipError("manager/manager.cc already exists");
        }
        if (path_exists(concat_tostr(dir, "/Simfile"))) {
            throw SipError("Simfile already exists");
        }
        init(init_args);
        SipPackage sp;
        sp.replace_variable_in_simfile("manager", "manager/manager.cc");
        sp.replace_variable_in_simfile("instances", "2", false);
        sp.replace_variable_in_simfile("max_concurrent", "1", false);
        sp.replace_variable_in_simfile("communication", "true", false);
        if (mkdir_r("manager") == -1 and errno != EEXIST) {
            THROW("mkdir_r()", errmsg());
        }
        put_file_contents("manager/manager.cc", from_unsafe{templates::communication_manager_cc()});
        return;
    }
    constexpr auto format = sim::ExecutionFormat::Communication;
    if (command == "test") {
        return test(args, format);
    }
    if (command == "genout") {
        throw SipError("communication problems have no output files; use sip gen");
    }
    if (args.size() != 0) {
        throw SipError("unexpected communication command argument: ", args.extract_next());
    }
    if (command == "gen") {
        return gen(format);
    }
    if (command == "regen") {
        return regen(format);
    }
    if (command == "pack") {
        SipPackage sp(format);
        sp.rebuild_full_simfile();
        return zip();
    }
    throw SipError("unknown communication command: ", command);
}

} // namespace commands
