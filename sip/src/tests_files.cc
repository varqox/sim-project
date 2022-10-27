#include "tests_files.hh"
#include "sip_error.hh"

static StringView get_test_name(StringView test_path) {
    test_path = test_path.substring(0, test_path.rfind('.'));
    return test_path.extract_trailing([](int c) { return c != '/'; });
}

TestsFiles::TestsFiles() {
    STACK_UNWINDING_MARK;

    pc.load_from_directory(".");
    pc.remove_with_prefix("utils/");

    pc.for_each_with_prefix("", [&](StringView file) {
        if (has_suffix(file, ".in")) {
            auto test_name = get_test_name(file);
            auto it = tests.find(test_name);
            if (it == tests.end()) {
                tests.emplace(test_name, file);
            } else if (it->second.in.has_value()) {
                throw SipError("input file of test ", it->first,
                        " was found in more than one location: ", it->second.in.value(), " and ",
                        file);
            } else {
                it->second.in = file;
            }

        } else if (has_suffix(file, ".out")) {
            auto test_name = get_test_name(file);
            auto it = tests.find(test_name);
            if (it == tests.end()) {
                tests.emplace(test_name, file);
            } else if (it->second.out.has_value()) {
                throw SipError("output file of test ", it->first,
                        " was found in more than one location: ", it->second.out.value(), " and ",
                        file);
            } else {
                it->second.out = file;
            }
        }
    });
}
