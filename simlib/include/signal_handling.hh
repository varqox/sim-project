#pragma once

#include "debug.hh"
#include "defer.hh"
#include "file_descriptor.hh"

#include <array>
#include <cassert>
#include <csignal>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <poll.h>
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
int handle_signals_while_running(Main&& main_func,
                                 Cleanup&& cleanup_before_getting_killed,
                                 Signals... signals) {
	static_assert(std::is_invocable_r_v<int, Main>,
	              "main_func has to take no arguments");
	static_assert(std::is_invocable_v<Cleanup, int>,
	              "cleanup_before_getting_killed has to take one argument -- "
	              "killing signal number");
	static_assert(
	   (std::is_same_v<Signals, int> and ...),
	   "signals parameters have to be from of SIGINT, SIGTERM, etc.");

	static FileDescriptor signal_pipe_write_end;
	assert(not signal_pipe_write_end.is_open() and
	       "Using this function simultaneously more than once is impossible");

	// Prepare signal pipe
	FileDescriptor signal_pipe_read_end;
	{
		std::array<int, 2> pfd;
		if (pipe2(pfd.data(), O_CLOEXEC | O_NONBLOCK))
			THROW("pipe2()", errmsg());

		signal_pipe_read_end = pfd[0];
		signal_pipe_write_end = pfd[1];
	}

	Defer pipe_write_end_closer = [&] { (void)signal_pipe_write_end.close(); };

	// Prepare pipe to signal that main_func has ended (needed because main
	// could end because of signal handler being run (e.g. because of some
	// EINTR), but after it is run, the signal-handling thread has no chance to
	// process it (e.g. process scheduling misfortune), so the thread that runs
	// main, should signal signal-handling thread that the main is complete and
	// wait for its confirmation that no signal happened or handle the signal)
	FileDescriptor main_func_ended_read_end;
	FileDescriptor main_func_ended_write_end;
	{
		std::array<int, 2> pfd;
		if (pipe2(pfd.data(), O_CLOEXEC | O_NONBLOCK))
			THROW("pipe2()", errmsg());

		main_func_ended_read_end = pfd[0];
		main_func_ended_write_end = pfd[1];
	}

	// static constexpr to ensure only one instance is created per outer
	// function instance
	static constexpr auto signal_handler = [](int signum) noexcept {
		int errnum = errno;
		(void)write(signal_pipe_write_end, &signum, sizeof(int));
		errno = errnum;
	};

	// Signal control
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = signal_handler;
	// Intercept signals, to allow a dedicated thread to consume them
	if ((sigaction(signals, &sa, nullptr) or ...))
		THROW("sigaction()", errmsg());

	// Spawn signal-handling thread
	std::thread signal_handling_thread(
	   [&, signal_pipe_read_end = std::move(signal_pipe_read_end),
	    main_func_ended_read_end = std::move(main_func_ended_read_end)] {
		   int signum;
		   // Wait for signal
		   constexpr int signal_pipe_idx = 0;
		   constexpr int main_func_ended_pipe_idx = 1;
		   std::array<pollfd, 2> pfds {{
		      {signal_pipe_read_end, POLLIN, 0},
		      {main_func_ended_read_end, POLLIN, 0},
		   }};
		   for (int rc;;) {
			   rc = poll(pfds.data(), 2, -1);
			   if (rc >= 1)
				   break; // Signal arrived or main has ended

			   assert(rc < 0);
			   if (errno != EINTR)
				   THROW("poll()");
		   }

		   for (auto const& pfd : pfds)
			   assert(not(pfd.revents & POLLHUP));

		   if (pfds[signal_pipe_idx].revents & POLLIN) {
			   int rc = read(signal_pipe_read_end, &signum, sizeof(int));
			   assert(rc == sizeof(int));

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
			   (void)kill(0, signum);
			   // Wait for signal to kill the process (just in case), repeatedly
			   // because other signals may happen in the meantime
			   for (;;)
				   pause();
		   }

		   assert((pfds[main_func_ended_pipe_idx].revents & POLLIN) and
		          "There should be no other cases than the two handled here: "
		          "signal or main_func ended");
		   return; // Signal did not happen before the main has ended, so we are
		           // done
	   });

	Defer before_return_or_exception = [&] {
		// Signal signal-handling thread that main has ended (normally or
		// abnormally)
		assert(write(main_func_ended_write_end, "x", 1) == 1);
		// Wait till the signal-handling thread kills the whole process or
		// confirms that it is safe to proceed
		signal_handling_thread.join();
	};
	return main_func();
}
