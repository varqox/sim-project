#include "assert_result.hh"

#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <seccomp.h>
#include <simlib/file_descriptor.hh>
#include <simlib/pipe.hh>
#include <simlib/sandbox/sandbox.hh>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>

// NOLINTNEXTLINE
TEST(sandbox, tracee_seccomp_negative_fd) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_THAT(
        [&] { sc.send_request({{"/bin/true"}}, {.seccomp_bpf_fd = -1}); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("sendmsg() - Bad file descriptor (os error 9)")
        )
    );
}

// NOLINTNEXTLINE
TEST(sandbox, tracee_seccomp_nonexistent_fd) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_THAT(
        [&] { sc.send_request({{"/bin/true"}}, {.seccomp_bpf_fd = 62356}); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("sendmsg() - Bad file descriptor (os error 9)")
        )
    );
}

// NOLINTNEXTLINE
TEST(sandbox, tracee_seccomp_nonseekable_fd) {
    auto pipe = pipe2(O_CLOEXEC);
    ASSERT_TRUE(pipe.has_value());
    auto sc = sandbox::spawn_supervisor();
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    sc.send_request({{"/bin/true"}}, {.seccomp_bpf_fd = pipe->readable});
    ASSERT_RESULT_ERROR(sc.await_result(), "tracee: lseek64() - Illegal seek (os error 29)");
}

// NOLINTNEXTLINE
TEST(sandbox, tracee_seccomp_zero_length_fd) {
    auto mfd = FileDescriptor{memfd_create("tracee_seccomp_test", MFD_CLOEXEC)};
    ASSERT_TRUE(mfd.is_open());
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{"/bin/true"}}, {.seccomp_bpf_fd = mfd});
    ASSERT_RESULT_ERROR(sc.await_result(), "tracee: mmap() - Invalid argument (os error 22)");
}

// NOLINTNEXTLINE
TEST(sandbox, tracee_seccomp_invalid_length_fd) {
    auto mfd = FileDescriptor{memfd_create("tracee_seccomp_test", MFD_CLOEXEC)};
    ASSERT_TRUE(mfd.is_open());
    ASSERT_EQ(ftruncate(mfd, 1), 0);
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{"/bin/true"}}, {.seccomp_bpf_fd = mfd});
    ASSERT_RESULT_ERROR(
        sc.await_result(), "tracee: invalid seccomp_bpf_fd length: 1 is not a multiple of 8"
    );
}

// NOLINTNEXTLINE
TEST(sandbox, tracee_seccomp_too_big_fd) {
    auto mfd = FileDescriptor{memfd_create("tracee_seccomp_test", MFD_CLOEXEC)};
    ASSERT_TRUE(mfd.is_open());
    ASSERT_EQ(ftruncate(mfd, 1 << 19), 0);
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{"/bin/true"}}, {.seccomp_bpf_fd = mfd});
    ASSERT_RESULT_ERROR(sc.await_result(), "tracee: seccomp_bpf_fd is too big");
}

// NOLINTNEXTLINE
TEST(sandbox, tracee_seccomp_invalid_seccomp_prog) {
    auto mfd = FileDescriptor{memfd_create("tracee_seccomp_test", MFD_CLOEXEC)};
    ASSERT_TRUE(mfd.is_open());
    ASSERT_EQ(ftruncate(mfd, 8), 0);
    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{"/bin/true"}}, {.seccomp_bpf_fd = mfd});
    ASSERT_RESULT_ERROR(sc.await_result(), "tracee: seccomp() - Invalid argument (os error 22)");
}

// NOLINTNEXTLINE
TEST(sandbox, tracee_seccomp_forbidden_execveat) {
    auto mfd = FileDescriptor{memfd_create("tracee_seccomp_test", MFD_CLOEXEC)};
    ASSERT_TRUE(mfd.is_open());

    auto seccomp_ctx = seccomp_init(SECCOMP_RET_ALLOW);
    ASSERT_TRUE(seccomp_ctx);
    ASSERT_EQ(
        seccomp_rule_add(seccomp_ctx, SCMP_ACT_ERRNO(EHOSTUNREACH), SCMP_SYS(execveat), 0), 0
    );
    ASSERT_EQ(seccomp_export_bpf(seccomp_ctx, mfd), 0);
    seccomp_release(seccomp_ctx);

    auto sc = sandbox::spawn_supervisor();
    sc.send_request({{"/bin/true"}}, {.seccomp_bpf_fd = mfd});
    ASSERT_RESULT_ERROR(sc.await_result(), "tracee: execveat() - No route to host (os error 113)");
}
