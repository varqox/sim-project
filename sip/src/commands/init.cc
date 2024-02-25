#include "../sip_error.hh"
#include "../sip_package.hh"
#include "commands.hh"

#include <simlib/macros/stack_unwinding.hh>

namespace commands {

void init(ArgvParser args) {
    STACK_UNWINDING_MARK;

    if (args.size() == 0) {
        throw SipError("missing directory argument - see -h for help");
    }

    auto specified_dir = args.extract_next();
    if (mkdir_r(specified_dir.to_string()) == -1 and errno != EEXIST) {
        throw SipError("failed to create directory: ", specified_dir, " (mkdir_r()", errmsg(), ')');
    }

    if (chdir(specified_dir.data()) == -1) {
        THROW("chdir()", errmsg());
    }

    SipPackage::create_default_directory_structure();
    SipPackage sp;
    sp.create_default_sipfile();

    if (args.size() > 0) {
        sp.create_default_simfile(args.extract_next());
    } else {
        sp.create_default_simfile(std::nullopt);
    }

    name({0, nullptr});
    mem({0, nullptr});
}

} // namespace commands
