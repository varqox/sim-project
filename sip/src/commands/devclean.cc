#include "../sip_package.hh"

#include <simlib/debug.hh>
#include <simlib/file_info.hh>

namespace commands {

void devclean() {
    STACK_UNWINDING_MARK;
    bool recreate_tests_dir = false;
    if (is_directory("tests/")) {
        recreate_tests_dir = true;
        // If tests/ becomes empty, then it will be removed and regenerated
        // tests will be placed inside in/, out/ directories. To prevent it we
        // need to retain this directory
    }

    SipPackage sp;
    sp.remove_generated_test_files();
    sp.clean();

    if (recreate_tests_dir) {
        (void)mkdir("tests/");
    }
}

} // namespace commands
