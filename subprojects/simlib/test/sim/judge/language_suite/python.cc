#include "fresh_execution.hh"
#include "run_in_fully_interpreted_language_suite.hh"

#include <gtest/gtest.h>
#include <simlib/directory.hh>
#include <simlib/file_info.hh>
#include <simlib/pipe.hh>
#include <simlib/sandbox/si.hh>
#include <simlib/sim/judge/language_suite/python.hh>

constexpr auto test_prog = R"(
import sys
sys.exit(int(sys.argv[1]))
)";

// NOLINTNEXTLINE
TEST(sim_judge_language_suite, python) {
    auto suite = sim::judge::language_suite::Python{};
    ASSERT_EQ(suite.is_supported(), path_exists("/usr/bin/python3"));
    ASSERT_TRUE(suite.is_supported());
    auto res = run_in_fully_intepreted_language_suite(suite, test_prog, {{"42"}});
    ASSERT_EQ(res.si, (sandbox::Si{.code = CLD_EXITED, .status = 42}));
}

// NOLINTNEXTLINE
TEST(sim_judge_language_suite, python_fresh_execution_isolates_ipc) {
    auto suite = sim::judge::language_suite::Python{};
    compile_for_fresh_execution(suite, R"(
import ctypes, errno, sys
libc = ctypes.CDLL(None, use_errno=True)
if sys.argv[1] == 'store':
    sem = libc.semget(173241, 1, 0o600 | 0o1000 | 0o2000)
    assert sem >= 0
    assert libc.semctl(sem, 0, 16, 42) == 0
else:
    assert libc.semget(173241, 1, 0o600) == -1
    assert ctypes.get_errno() == errno.ENOENT
)");
    auto program = suite.prepared_program();
    auto first = program.start_fresh({{"store"}}, fresh_execution_options());
    ASSERT_EQ(first.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 0}));
    auto second = program.start_fresh({{"load"}}, fresh_execution_options());
    ASSERT_EQ(second.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 0}));
}

// NOLINTNEXTLINE
TEST(sim_judge_language_suite, python_prepared_program_owns_source) {
    auto program = [] {
        auto suite = sim::judge::language_suite::Python{};
        compile_for_fresh_execution(suite, "raise SystemExit(42)\n");
        auto prepared = suite.prepared_program();
        compile_for_fresh_execution(suite, "raise SystemExit(17)\n");
        return prepared;
    }();
    auto execution = program.start_fresh({}, fresh_execution_options());
    ASSERT_EQ(execution.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 42}));
    ASSERT_THROW(execution.await_result(), std::exception);
}

// NOLINTNEXTLINE
TEST(sim_judge_language_suite, python_fresh_executions_run_concurrently) {
    auto suite = sim::judge::language_suite::Python{};
    compile_for_fresh_execution(suite, R"(
import sys
print('r', end='', flush=True)
assert sys.stdin.read(1) == 'x'
)");
    auto program = suite.prepared_program();
    auto input1 = pipe2(O_CLOEXEC).value();
    auto input2 = pipe2(O_CLOEXEC).value();
    auto output1 = pipe2(O_CLOEXEC).value();
    auto output2 = pipe2(O_CLOEXEC).value();
    auto options = fresh_execution_options();
    options.stdin_fd = input1.readable;
    options.stdout_fd = output1.writable;
    auto first = program.start_fresh({}, options);
    options.stdin_fd = input2.readable;
    options.stdout_fd = output2.writable;
    auto second = program.start_fresh({}, options);
    ASSERT_TRUE(readable_within(first.started_fd(), std::chrono::seconds{10}));
    ASSERT_TRUE(readable_within(second.started_fd(), std::chrono::seconds{10}));
    ASSERT_TRUE(readable_within(output1.readable, std::chrono::seconds{10}));
    ASSERT_TRUE(readable_within(output2.readable, std::chrono::seconds{10}));
    ASSERT_EQ(get_file_contents(output1.readable, 1), "r");
    ASSERT_EQ(get_file_contents(output2.readable, 1), "r");
    ASSERT_EQ(write(input1.writable, "x", 1), 1);
    ASSERT_EQ(write(input2.writable, "x", 1), 1);
    ASSERT_EQ(first.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 0}));
    ASSERT_EQ(second.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 0}));
}

// NOLINTNEXTLINE
TEST(sim_judge_language_suite, python_fresh_execution_kill_and_cancel) {
    auto suite = sim::judge::language_suite::Python{};
    compile_for_fresh_execution(suite, "import time\ntime.sleep(20)\n");
    auto program = suite.prepared_program();
    auto execution = program.start_fresh({}, fresh_execution_options());
    ASSERT_TRUE(readable_within(execution.started_fd(), std::chrono::seconds{10}));
    execution.kill();
    ASSERT_EQ(execution.await_result().si, (sandbox::Si{.code = CLD_KILLED, .status = SIGKILL}));
    auto cancelled = program.start_fresh({}, fresh_execution_options());
    cancelled.cancel();
    ASSERT_THROW(cancelled.await_result(), std::exception);
}

// NOLINTNEXTLINE
TEST(sim_judge_language_suite, python_execution_outlives_prepared_program) {
    auto execution = [] {
        auto suite = sim::judge::language_suite::Python{};
        compile_for_fresh_execution(suite, "raise SystemExit(42)\n");
        return suite.prepared_program().start_fresh({}, fresh_execution_options());
    }();
    auto moved_execution = std::move(execution);
    ASSERT_EQ(moved_execution.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 42}));
}

// NOLINTNEXTLINE
TEST(sim_judge_language_suite, python_cannot_prepare_without_compilation) {
    auto suite = sim::judge::language_suite::Python{};
    ASSERT_THROW((void)suite.prepared_program(), std::exception);
}

// NOLINTNEXTLINE
TEST(sim_judge_language_suite, python_fresh_execution_releases_descriptors_and_children) {
    auto suite = sim::judge::language_suite::Python{};
    compile_for_fresh_execution(suite, "pass\n");
    auto program = suite.prepared_program();
    auto count_fds = [] {
        size_t count = 0;
        for_each_dir_component("/proc/self/fd", [&](dirent*) { ++count; });
        return count;
    };
    auto children_path = concat_tostr("/proc/self/task/", gettid(), "/children");
    auto children = get_file_contents(children_path);
    auto fds = count_fds();
    for (int i = 0; i < 12; ++i) {
        {
            auto execution = program.start_fresh({}, fresh_execution_options());
            if (i % 3 == 0) {
                execution.cancel();
            } else if (i % 3 == 1) {
                ASSERT_EQ(
                    execution.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 0})
                );
            }
        }
        ASSERT_EQ(count_fds(), fds);
        ASSERT_EQ(get_file_contents(children_path), children);
    }
}
