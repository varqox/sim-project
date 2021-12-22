#pragma once

#include "simlib/debug.hh"
#include "simlib/defer.hh"
#include "simlib/file_descriptor.hh"

#include <array>
#include <cassert>
#include <csignal>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <thread>
#include <type_traits>
#include <unistd.h>

/**
 * @brief Runs @p main_func and while doing it, handle signals @p signals
 * @details It is assumed that every one of @p signals with SIG_DFL will cause
 *   the process to terminate.
 *
 * @param main_func Function to run after setting signal handling
 * @param cleanup_before_getting_killed Function to run on receiving signal just
 *   before killing the process. It will be run in a separate thread and
 *   should not throw, because all exceptions are ignored.
 * @param signals Signals to handle (i.e. intercept)
 *
 * @return value returned by @p main_func if no signal arrived. Otherwise
 *   whole process is killed by intercepted signal and this function
 *   does not return.
 */
template <class Main, class Cleanup, class... Signals>
auto handle_signals_while_running(
        Main&& main_func, Cleanup&& cleanup_before_getting_killed, Signals... signals);

class HandleSignalsWhileRunning {
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    inline static FileDescriptor signal_eventfd{};

    static uint64_t pack_signum(int signum) noexcept {
        static_assert(sizeof(int) == 4, "Needed for the below hack to work properly");
        uint32_t usignum = signum;
        return (static_cast<uint64_t>(1) << 32 | usignum);
    };

    static int unpack_signum(uint64_t packed_signum) noexcept {
        static_assert(sizeof(int) == 4, "Needed for the below hack to work properly");
        return static_cast<uint32_t>(packed_signum & ((static_cast<uint64_t>(1) << 32) - 1));
    };

    static void signal_handler(int signum) noexcept {
        int errnum = errno;
        uint64_t packed_signum = pack_signum(signum);
        (void)write(signal_eventfd, &packed_signum, sizeof(packed_signum));
        errno = errnum;
    };

public:
    template <class Main, class Cleanup, class... Signals>
    friend auto handle_signals_while_running(
            Main&& main_func, Cleanup&& cleanup_before_getting_killed, Signals... signals) {
        static_assert(std::is_invocable_v<Main>, "main_func has to take no arguments");
        static_assert(std::is_invocable_v<Cleanup, int>,
                "cleanup_before_getting_killed has to take one argument -- "
                "killing signal number");
        static_assert((std::is_same_v<Signals, int> and ...),
                "signals parameters have to be from of SIGINT, SIGTERM, etc.");

        assert(not HandleSignalsWhileRunning::signal_eventfd.is_open() and
                "This function is already running, using this function "
                "simultaneously is impossible");

        // Prepare signal eventfd
        HandleSignalsWhileRunning::signal_eventfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (not HandleSignalsWhileRunning::signal_eventfd.is_open()) {
            THROW("eventfd()", errmsg());
        }

        Defer signal_eventfd_closer = [] {
            (void)HandleSignalsWhileRunning::signal_eventfd.close();
        };

        // Prepare eventfd to signal that main_func has ended (needed because
        // main could end because of signal handler being run (e.g. because of
        // some EINTR), but after it is run, the signal-handling thread has no
        // chance to process it (e.g. process scheduling misfortune), so the
        // thread that runs main, should signal signal-handling thread that the
        // main is complete and wait for its confirmation that no signal
        // happened or handle the signal)
        FileDescriptor main_func_ended_eventfd{eventfd(0, O_CLOEXEC | EFD_NONBLOCK)};
        if (not main_func_ended_eventfd.is_open()) {
            THROW("eventfd()", errmsg());
        }

        // Signal control
        struct sigaction sa {};
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = HandleSignalsWhileRunning::signal_handler;
        // Intercept signals, to allow a dedicated thread to consume them
        if ((sigaction(signals, &sa, nullptr) or ...)) {
            THROW("sigaction()", errmsg());
        }

        // Spawn signal-handling thread
        std::thread signal_handling_thread([&] {
            // Wait for signal
            constexpr int signal_efd_idx = 0;
            constexpr int main_func_ended_efd_idx = 1;
            std::array<pollfd, 2> pfds{{
                    {HandleSignalsWhileRunning::signal_eventfd, POLLIN, 0},
                    {main_func_ended_eventfd, POLLIN, 0},
            }};
            for (int rc = 0;;) {
                rc = poll(pfds.data(), 2, -1);
                if (rc >= 1) {
                    break; // Signal arrived or main has ended
                }

                assert(rc < 0);
                if (errno != EINTR) {
                    THROW("poll()");
                }
            }

            for (auto const& pfd : pfds) {
                assert(not(pfd.revents & (POLLHUP | POLLERR)));
            }

            if (pfds[signal_efd_idx].revents & POLLIN) {
                uint64_t packed_signum = 0;
                auto read_bytes = read(HandleSignalsWhileRunning::signal_eventfd,
                        &packed_signum, sizeof(packed_signum));
                assert(read_bytes == sizeof(packed_signum));
                int signum = HandleSignalsWhileRunning::unpack_signum(packed_signum);

                try {
                    cleanup_before_getting_killed(signum);
                } catch (...) {
                }

                // Now kill the whole process group with the intercepted signal
                memset(&sa, 0, sizeof(sa));
                sa.sa_handler = SIG_DFL;
                // First unblock the blocked signal, so that it will kill the
                // process
                (void)sigaction(signum, &sa, nullptr);
                (void)kill(getpid(), signum);
                // Wait for signal to kill the process (just in case),
                // repeatedly because other signals may happen in the meantime
                for (;;) {
                    pause();
                }
            }

            assert((pfds[main_func_ended_efd_idx].revents & POLLIN) and
                    "There should be no other cases than the two handled here: "
                    "signal or main_func ended");
            // Signal did not happen before the main has ended, so we are done
        });

        Defer before_return_or_exception = [&] {
            // Signal signal-handling thread that main has ended (normally or
            // abnormally)
            uint64_t one = 1;
            auto written_bytes = write(main_func_ended_eventfd, &one, sizeof(one));
            assert(written_bytes == 8);
            // Wait till the signal-handling thread kills the whole process or
            // confirms that it is safe to proceed
            signal_handling_thread.join();
        };
        return main_func();
    }
};
