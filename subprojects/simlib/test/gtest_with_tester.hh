#pragma once

#include <cassert>
#include <gtest/gtest.h>
#include <string_view>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::string_view tester_executable_path;

// NOLINTNEXTLINE(misc-definitions-in-headers)
int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    assert(argc == 2);
    tester_executable_path = argv[1];
    return RUN_ALL_TESTS();
}
