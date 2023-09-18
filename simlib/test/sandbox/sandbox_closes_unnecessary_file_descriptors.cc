#include "../gtest_with_tester.hh"
#include "assert_result.hh"

#include <fcntl.h>
#include <simlib/file_descriptor.hh>
#include <simlib/pipe.hh>
#include <simlib/sandbox/sandbox.hh>
#include <unistd.h>

// NOLINTNEXTLINE
TEST(sandbox, sandbox_closes_stray_file_descriptor) {
    auto pipe = pipe2(O_NONBLOCK).value(); // NOLINT(bugprone-unchecked-optional-access)
    auto sc = sandbox::spawn_supervisor();
    // Wait for supervisor to close the other end of the pipe
    sc.await_result(sc.send_request({{"/bin/true"}}));
    // Check that supervisor closed the write end of the pipe
    ASSERT_EQ(pipe.writable.close(), 0);
    char buff[1];
    ASSERT_EQ(read(pipe.readable, buff, sizeof(buff)), 0);
}

// NOLINTNEXTLINE
TEST(sandbox, no_file_descriptor_leaks_to_sandboxed_process) {
    // Open a stray file descriptor without O_CLOEXEC
    FileDescriptor fd{open("/dev/zero", O_RDWR)}; // NOLINT(android-cloexec-open)
    ASSERT_TRUE(fd.is_open());
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request({{tester_executable_path}}, {.stderr_fd = STDERR_FILENO})),
        CLD_EXITED,
        0
    );
}
