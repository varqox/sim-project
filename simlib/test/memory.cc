#include "simlib/memory.hh"

#include <gtest/gtest.h>

using std::unique_ptr;

// NOLINTNEXTLINE
TEST(DISABLED_memory, delete_using_free) {
    (void)unique_ptr<char[], delete_using_free>(
        static_cast<char*>(malloc(42))); // NOLINT(cppcoreguidelines-no-malloc)

    (void)unique_ptr<struct stat, delete_using_free>(static_cast<struct stat*>(
        malloc(sizeof(struct stat)))); // NOLINT(cppcoreguidelines-no-malloc)

    // TODO: add some mocking
}
