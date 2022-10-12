#include "simlib/signal_handling.hh"
#include "simlib/concurrent/semaphore.hh"

#include <chrono>
#include <csignal>
#include <gtest/gtest-death-test.h>
#include <gtest/gtest.h>
#include <thread>
#include <unistd.h>

inline void eputs(const char* str) noexcept { write(STDERR_FILENO, str, std::strlen(str)); }

// NOLINTNEXTLINE
TEST(handle_signals_while_running_DeathTest, no_signal_occurred) {
    EXPECT_EXIT(
            {
                eputs("beg");
                handle_signals_while_running(
                        [] { eputs(",main"); }, [](int) { eputs(",sig"); }, SIGINT);
                eputs(",end");
                exit(0);
            },
            ::testing::ExitedWithCode(0), "^beg,main,end$");
}

// NOLINTNEXTLINE
TEST(handle_signals_while_running_DeathTest, two_signals_one_signaled) {
    for (int signum : {SIGINT, SIGTERM}) {
        EXPECT_EXIT(
                {
                    eputs("beg");
                    handle_signals_while_running(
                            [&] {
                                eputs(",main");
                                kill(getpid(), signum);
                            },
                            [](int) { eputs(",sig"); }, SIGINT, SIGTERM);
                    eputs(",end");
                    exit(0);
                },
                ::testing::KilledBySignal(signum), "^beg,main,sig$");
    }
}

// NOLINTNEXTLINE
TEST(handle_signals_while_running_DeathTest, signaled_unhandled_signal) {
    EXPECT_EXIT(
            {
                eputs("beg");
                handle_signals_while_running(
                        [] {
                            eputs(",main");
                            kill(getpid(), SIGQUIT);
                        },
                        [](int) { eputs(",sig"); }, SIGINT, SIGTERM);
                eputs(",end");
                exit(0);
            },
            ::testing::KilledBySignal(SIGQUIT), "^beg,main$");
}

// NOLINTNEXTLINE
TEST(handle_signals_while_running_DeathTest, try_to_run_two_times_simultaneously) {
    EXPECT_EXIT(
            {
                eputs("beg");
                handle_signals_while_running(
                        [] {
                            eputs(",main@");
                            handle_signals_while_running(
                                    [] { eputs(",main2"); }, [](int) { eputs(",sig2"); }, SIGQUIT);
                        },
                        [](int) { eputs(",sig"); }, SIGINT, SIGTERM);
                eputs(",end");
                exit(0);
            },
            ::testing::KilledBySignal(SIGABRT),
            "^beg,main@.*This function is already running, using this function .*"
            "simultaneously is impossible");
}

// NOLINTNEXTLINE
TEST(handle_signals_while_running_DeathTest, two_times_one_after_another) {
    EXPECT_EXIT(
            {
                eputs("beg");
                handle_signals_while_running(
                        [] { eputs(",main1"); }, [](int) { eputs(",sig1"); }, SIGINT, SIGTERM);
                eputs(",mid");
                handle_signals_while_running(
                        [] { eputs(",main2"); }, [](int) { eputs(",sig2"); }, SIGINT, SIGTERM);
                eputs(",end");
                exit(0);
            },
            ::testing::ExitedWithCode(0), "^beg,main1,mid,main2,end$");
}

// NOLINTNEXTLINE
TEST(handle_signals_while_running_DeathTest, two_times_one_after_another_first_signaled) {
    EXPECT_EXIT(
            {
                eputs("beg");
                handle_signals_while_running(
                        [] {
                            eputs(",main1");
                            kill(getpid(), SIGTERM);
                        },
                        [](int) { eputs(",sig1"); }, SIGINT, SIGTERM);
                eputs(",mid");
                handle_signals_while_running(
                        [] { eputs(",main2"); }, [](int) { eputs(",sig2"); }, SIGINT, SIGTERM);
                eputs(",end");
                exit(0);
            },
            ::testing::KilledBySignal(SIGTERM), "^beg,main1,sig1$");
}

// NOLINTNEXTLINE
TEST(handle_signals_while_running_DeathTest, two_times_one_after_another_second_signaled) {
    EXPECT_EXIT(
            {
                eputs("beg");
                handle_signals_while_running(
                        [] { eputs(",main1"); }, [](int) { eputs(",sig1"); }, SIGINT, SIGTERM);
                eputs(",mid");
                handle_signals_while_running(
                        [] {
                            eputs(",main2");
                            kill(getpid(), SIGTERM);
                        },
                        [](int) { eputs(",sig2"); }, SIGINT, SIGTERM);
                eputs(",end");
                exit(0);
            },
            ::testing::KilledBySignal(SIGTERM), "^beg,main1,mid,main2,sig2$");
}

// NOLINTNEXTLINE
TEST(handle_signals_while_running_DeathTest, function_throws_exception) {
    EXPECT_EXIT(
            {
                eputs("beg");
                try {
                    handle_signals_while_running(
                            [] {
                                eputs(",main");
                                throw 42; // NOLINT(hicpp-exception-baseclass)
                            },
                            [](int) { eputs(",sig"); }, SIGINT, SIGTERM);
                } catch (int x) {
                    eputs(",catch");
                    assert(x == 42);
                }
                eputs(",end");
                exit(0);
            },
            ::testing::ExitedWithCode(0), "^beg,main,catch,end$");
}

// NOLINTNEXTLINE
TEST(handle_signals_while_running_DeathTest, function_returns_before_signal_gets_handled) {
    EXPECT_EXIT(
            {
                eputs("beg");
                concurrent::Semaphore sem(0);
                handle_signals_while_running(
                        [&] {
                            Defer poster = [&] { sem.post(); };
                            kill(getpid(), SIGUSR1);
                            eputs(",main");
                        },
                        [&](int) {
                            sem.wait();
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                            eputs(",sig");
                        },
                        SIGUSR1, SIGUSR2);
                eputs(",end");
                exit(0);
            },
            ::testing::KilledBySignal(SIGUSR1), "^beg,main,sig$");
}

// NOLINTNEXTLINE
TEST(handle_signals_while_running_DeathTest, signal_with_function_that_does_not_return) {
    auto start_tp = std::chrono::system_clock::now();
    EXPECT_EXIT(
            {
                eputs("beg");
                handle_signals_while_running(
                        [] {
                            eputs(",main");
                            kill(getpid(), SIGUSR2);
                            std::this_thread::sleep_for(std::chrono::seconds(4));
                        },
                        [](int) { eputs(",sig"); }, SIGUSR1, SIGUSR2);
                eputs(",end");
                exit(0);
            },
            ::testing::KilledBySignal(SIGUSR2), "^beg,main,sig$");
    // Ensure that the above test interrupted the main function successfully
    auto end_tp = std::chrono::system_clock::now();
    EXPECT_LT(end_tp - start_tp, std::chrono::seconds(1));
}
