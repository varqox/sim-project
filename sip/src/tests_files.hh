#pragma once

#include "simlib/sim/problem_package.hh"

#include <map>
#include <optional>

struct TestsFiles {
    struct Test {
        std::optional<StringView> in, out;
        explicit Test(StringView test_path) {
            if (has_suffix(test_path, ".in")) {
                in = test_path;
            } else {
                out = test_path;
            }
        }
    };

    sim::PackageContents pc;
    std::map<StringView, Test> tests; // test name => test

    TestsFiles();
};
