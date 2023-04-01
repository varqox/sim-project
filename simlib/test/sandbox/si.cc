#include <csignal>
#include <gtest/gtest.h>
#include <simlib/sandbox/si.hh>
#include <sys/wait.h>

using sandbox::Si;

// NOLINTNEXTLINE
TEST(sandbox, si_description_exited) {
    EXPECT_EQ((Si{.code = CLD_EXITED, .status = 0}).description(), "exited with 0");
    EXPECT_EQ((Si{.code = CLD_EXITED, .status = 42}).description(), "exited with 42");
}

// NOLINTNEXTLINE
TEST(sandbox, si_description_killed) {
    EXPECT_EQ(
        (Si{.code = CLD_KILLED, .status = SIGKILL}).description(), "killed by signal KILL - Killed"
    );
    EXPECT_EQ(
        (Si{.code = CLD_KILLED, .status = SIGSEGV}).description(),
        "killed by signal SEGV - Segmentation fault"
    );
    EXPECT_EQ(
        (Si{.code = CLD_KILLED, .status = SIGINT}).description(), "killed by signal INT - Interrupt"
    );
    EXPECT_EQ(
        (Si{.code = CLD_KILLED, .status = 0}).description(), "killed by signal with number 0"
    );
}

// NOLINTNEXTLINE
TEST(sandbox, si_description_dumped) {
    EXPECT_EQ(
        (Si{.code = CLD_DUMPED, .status = SIGKILL}).description(),
        "killed and dumped by signal KILL - Killed"
    );
    EXPECT_EQ(
        (Si{.code = CLD_DUMPED, .status = SIGSEGV}).description(),
        "killed and dumped by signal SEGV - Segmentation fault"
    );
    EXPECT_EQ(
        (Si{.code = CLD_DUMPED, .status = SIGINT}).description(),
        "killed and dumped by signal INT - Interrupt"
    );
    EXPECT_EQ(
        (Si{.code = CLD_DUMPED, .status = 0}).description(),
        "killed and dumped by signal with number 0"
    );
}

// NOLINTNEXTLINE
TEST(sandbox, si_description_trapped) {
    EXPECT_EQ(
        (Si{.code = CLD_TRAPPED, .status = SIGKILL}).description(),
        "trapped by signal KILL - Killed"
    );
    EXPECT_EQ(
        (Si{.code = CLD_TRAPPED, .status = SIGSEGV}).description(),
        "trapped by signal SEGV - Segmentation fault"
    );
    EXPECT_EQ(
        (Si{.code = CLD_TRAPPED, .status = SIGINT}).description(),
        "trapped by signal INT - Interrupt"
    );
    EXPECT_EQ(
        (Si{.code = CLD_TRAPPED, .status = SIGTRAP}).description(),
        "trapped by signal TRAP - Trace/breakpoint trap"
    );
    EXPECT_EQ(
        (Si{.code = CLD_TRAPPED, .status = 0}).description(), "trapped by signal with number 0"
    );
}

// NOLINTNEXTLINE
TEST(sandbox, si_description_stopped) {
    EXPECT_EQ(
        (Si{.code = CLD_STOPPED, .status = SIGSTOP}).description(),
        "stopped by signal STOP - Stopped (signal)"
    );
    EXPECT_EQ(
        (Si{.code = CLD_STOPPED, .status = SIGTSTP}).description(),
        "stopped by signal TSTP - Stopped"
    );
    EXPECT_EQ(
        (Si{.code = CLD_STOPPED, .status = SIGTTIN}).description(),
        "stopped by signal TTIN - Stopped (tty input)"
    );
    EXPECT_EQ(
        (Si{.code = CLD_STOPPED, .status = SIGTTOU}).description(),
        "stopped by signal TTOU - Stopped (tty output)"
    );
    EXPECT_EQ(
        (Si{.code = CLD_STOPPED, .status = 0}).description(), "stopped by signal with number 0"
    );
}

// NOLINTNEXTLINE
TEST(sandbox, si_description_continued) {
    EXPECT_EQ(
        (Si{.code = CLD_CONTINUED, .status = SIGCONT}).description(),
        "continued by signal CONT - Continued"
    );
    EXPECT_EQ(
        (Si{.code = CLD_CONTINUED, .status = SIGKILL}).description(),
        "continued by signal KILL - Killed"
    );
    EXPECT_EQ(
        (Si{.code = CLD_CONTINUED, .status = SIGINT}).description(),
        "continued by signal INT - Interrupt"
    );
    EXPECT_EQ(
        (Si{.code = CLD_CONTINUED, .status = 0}).description(), "continued by signal with number 0"
    );
}

// NOLINTNEXTLINE
TEST(sandbox, si_description_invalid) {
    EXPECT_EQ(
        (Si{.code = 6135, .status = 18258}).description(),
        "unable to describe (code 6135, status 18258)"
    );
}

// NOLINTNEXTLINE
TEST(sandbox, si_operator_equals) {
    EXPECT_TRUE((Si{.code = 0, .status = 1} == Si{.code = 0, .status = 1}));
    EXPECT_FALSE((Si{.code = 0, .status = 1} == Si{.code = 0, .status = 2}));
    EXPECT_FALSE((Si{.code = 0, .status = 1} == Si{.code = 1, .status = 0}));
    EXPECT_FALSE((Si{.code = 0, .status = 1} == Si{.code = 0, .status = 0}));

    EXPECT_TRUE((Si{.code = 6542, .status = 65218} == Si{.code = 6542, .status = 65218}));
    EXPECT_FALSE((Si{.code = 6542, .status = 65218} == Si{.code = 4628, .status = 65218}));
    EXPECT_FALSE((Si{.code = 6542, .status = 65218} == Si{.code = 6542, .status = 71}));
}

// NOLINTNEXTLINE
TEST(sandbox, si_operator_not_equals) {
    EXPECT_FALSE((Si{.code = 0, .status = 1} != Si{.code = 0, .status = 1}));
    EXPECT_TRUE((Si{.code = 0, .status = 1} != Si{.code = 0, .status = 2}));
    EXPECT_TRUE((Si{.code = 0, .status = 1} != Si{.code = 1, .status = 0}));
    EXPECT_TRUE((Si{.code = 0, .status = 1} != Si{.code = 0, .status = 0}));

    EXPECT_FALSE((Si{.code = 6542, .status = 65218} != Si{.code = 6542, .status = 65218}));
    EXPECT_TRUE((Si{.code = 6542, .status = 65218} != Si{.code = 4628, .status = 65218}));
    EXPECT_TRUE((Si{.code = 6542, .status = 65218} != Si{.code = 6542, .status = 71}));
}
