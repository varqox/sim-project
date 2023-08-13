#include "../sip_package.hh"
#include "commands.hh"

#include <simlib/macros/stack_unwinding.hh>
#include <simlib/path.hh>
#include <simlib/working_directory.hh>

namespace commands {

void zip() {
    STACK_UNWINDING_MARK;
    clean();
    auto cwd = get_cwd();
    --cwd.size;
    SipPackage::archive_into_zip(path_filename(cwd.to_cstr()));
}

} // namespace commands
