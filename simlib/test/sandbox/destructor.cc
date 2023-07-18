#include <gtest/gtest.h>
#include <simlib/sandbox/sandbox.hh>

// NOLINTNEXTLINE
TEST(sandbox, sandbox_destructs_cleanly_after_no_requests) {
    auto sc = sandbox::spawn_supervisor();
}
