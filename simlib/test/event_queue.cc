#include "simlib/event_queue.hh"
#include "simlib/concurrent/semaphore.hh"
#include "simlib/defer.hh"
#include "simlib/file_descriptor.hh"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <gtest/gtest-death-test.h>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <thread>
#include <type_traits>
#include <unistd.h>

using std::array;
using std::chrono::milliseconds;
using std::chrono::system_clock;
using std::chrono_literals::operator""ms;
using std::chrono_literals::operator""us;
using std::chrono_literals::operator""ns;
using std::function;
using std::string;
using std::vector;

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, add_time_handler_time_point) {
	auto start = system_clock::now();
	string order;

	EventQueue eq;
	eq.add_time_handler(start + 3ms, [&] {
		EXPECT_LE(start + 3ms, system_clock::now());
		order += "3";

		eq.add_time_handler(start + 6ms, [&] {
			EXPECT_LE(start + 6ms, system_clock::now());
			order += "6";
		});
	});

	eq.add_time_handler(start + 2ms, [&] {
		EXPECT_LE(start + 2ms, system_clock::now());
		order += "2";

		eq.add_time_handler(start + 4ms, [&] {
			EXPECT_LE(start + 4ms, system_clock::now());
			order += "4";
		});
	});

	eq.add_time_handler(start + 5ms, [&] {
		EXPECT_LE(start + 5ms, system_clock::now());
		order += "5";
	});

	eq.run();
	EXPECT_EQ(order, "23456");
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, add_time_handler_duration) {
	auto start = system_clock::now();
	string order;

	EventQueue eq;
	eq.add_time_handler(3ms, [&] {
		EXPECT_LE(start + 3ms, system_clock::now());
		order += "3";

		eq.add_time_handler(3ms, [&] {
			EXPECT_LE(start + 6ms, system_clock::now());
			order += "6";
		});
	});

	eq.add_time_handler(2ms, [&] {
		EXPECT_LE(start + 2ms, system_clock::now());
		order += "2";

		eq.add_time_handler(2ms, [&] {
			EXPECT_LE(start + 4ms, system_clock::now());
			order += "4";
		});
	});

	eq.add_time_handler(5ms, [&] {
		EXPECT_LE(start + 5ms, system_clock::now());
		order += "5";
	});

	eq.run();
	EXPECT_EQ(order, "23456");
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, add_ready_handler) {
	auto start = system_clock::now();
	string order;

	EventQueue eq;
	eq.add_ready_handler([&] { order += "0"; });

	eq.add_time_handler(1ms, [&] {
		EXPECT_LE(start + 1ms, system_clock::now());
		order += "1";
	});

	eq.add_ready_handler([&] {
		order += "0"; // Order of handlers on the same time_point is undefined
	});

	eq.run();
	EXPECT_EQ(order, "001");
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, add_repeating_handler) {
	auto start = system_clock::now();
	string order;

	EventQueue eq;
	eq.add_repeating_handler(100us, [&, iter = 0]() mutable {
		order += char('a' + iter);
		++iter;

		EXPECT_LE(start + iter * 100us, system_clock::now());
		return iter == 10 ? stop_repeating : continue_repeating;
	});

	eq.add_time_handler(50us, [&] {
		EXPECT_LE(start + 50us, system_clock::now());
		order += "1";
	});

	eq.add_time_handler(150us, [&] {
		EXPECT_LE(start + 150us, system_clock::now());
		order += "2";
	});

	eq.run();
	EXPECT_EQ(order, "1a2bcdefghij");
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, adding_mutable_handler) {
	EventQueue eq;
	size_t times = 0;

	eq.add_ready_handler([&, x = 0]() mutable {
		++x;
		++times;
	});

	eq.add_time_handler(0ns, [&, x = 0]() mutable {
		++x;
		++times;
	});

	eq.add_time_handler(system_clock::now(), [&, x = 0]() mutable {
		++x;
		++times;
	});

	eq.add_repeating_handler(0ns, [&, x = 0]() mutable {
		++x;
		++times;
		return stop_repeating;
	});

	FileDescriptor fd("/dev/null", O_RDONLY);
	assert(fd.is_open());
	EventQueue::handler_id_t hid = 0;
	hid = eq.add_file_handler(fd, FileEvent::READABLE, [&, x = 0]() mutable {
		++x;
		++times;
		eq.remove_handler(hid);
	});

	eq.run();
	EXPECT_EQ(times, 5);
}

template <class Stopper>
static void pause_immediately_from_handler(Stopper&& stopper_installer) {
	static_assert(std::is_invocable_v<Stopper, EventQueue&, size_t&>);

	EventQueue eq;
	size_t times = 0;

	auto handler_impl = [&](auto& self) -> void {
		++times;
		eq.add_ready_handler([&self] { self(self); });
	};

	eq.add_ready_handler([&] { handler_impl(handler_impl); });

	FileDescriptor fd("/dev/null", O_WRONLY);
	assert(fd.is_open());
	eq.add_file_handler(fd, FileEvent::WRITEABLE, [&] { ++times; });

	eq.add_repeating_handler(0ns, [&] {
		++times;
		return continue_repeating;
	});

	stopper_installer(eq, times);

	eq.run();
	EXPECT_EQ(times, 0);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, pause_immediately_from_time_handler) {
	pause_immediately_from_handler([](EventQueue& eq, size_t& times) {
		eq.add_time_handler(100us, [&] {
			times = 0;
			eq.pause_immediately();
		});
	});
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, pause_immediately_from_repeating_handler) {
	pause_immediately_from_handler([](EventQueue& eq, size_t& times) {
		eq.add_repeating_handler(8us, [&, iter = 0]() mutable {
			if (++iter == 10) {
				times = 0;
				eq.pause_immediately();
			}
			return continue_repeating;
		});
	});
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, pause_immediately_from_file_handler) {
	FileDescriptor fd("/dev/null", O_WRONLY);
	pause_immediately_from_handler([&](EventQueue& eq, size_t& times) {
		eq.add_file_handler(fd, FileEvent::WRITEABLE, [&, iter = 0]() mutable {
			if (++iter == 4) {
				times = 0;
				eq.pause_immediately();
			}
			return true;
		});
	});
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue,
     pause_immediately_from_other_thread_while_running_time_handlers) {
	EventQueue eq;
	size_t times = 0;

	auto handler_impl = [&](auto& self) -> void {
		++times;
		eq.add_ready_handler([&self] { self(self); });
	};

	eq.add_ready_handler([&] { handler_impl(handler_impl); });

	concurrent::Semaphore sem(0);
	eq.add_ready_handler([&] { sem.post(); });

	std::thread other([&] {
		sem.wait();
		std::this_thread::sleep_for(8us);
		eq.pause_immediately();
	});

	eq.run();
	other.join();

	EXPECT_GT(times, 0);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue,
     pause_immediately_from_other_thread_while_running_repeating_handler) {
	EventQueue eq;
	size_t times = 0;

	eq.add_repeating_handler(0ns, [&] {
		++times;
		return continue_repeating;
	});

	concurrent::Semaphore sem(0);
	eq.add_ready_handler([&] { sem.post(); });

	std::thread other([&] {
		sem.wait();
		std::this_thread::sleep_for(8us);
		eq.pause_immediately();
	});

	eq.run();
	other.join();

	EXPECT_GT(times, 0);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue,
     pause_immediately_from_other_thread_while_running_file_handlers) {
	EventQueue eq;
	size_t writes = 0;

	FileDescriptor fd("/dev/null", O_WRONLY);
	assert(fd.is_open());
	eq.add_file_handler(fd, FileEvent::WRITEABLE, [&] { ++writes; });

	concurrent::Semaphore sem(0);
	eq.add_ready_handler([&] { sem.post(); });

	std::thread other([&] {
		sem.wait();
		std::this_thread::sleep_for(8us);
		eq.pause_immediately();
	});

	eq.run();
	other.join();

	EXPECT_GT(writes, 0);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, pause_immediately_from_other_thread_while_waiting) {
	EventQueue eq;
	string order;

	std::array<int, 2> pfd{};
	if (pipe2(pfd.data(), O_CLOEXEC)) {
		THROW("pipe2()", errmsg());
	}

	Defer guard = [&pfd] {
		for (int fd : pfd) {
			(void)close(fd);
		}
	};

	eq.add_file_handler(pfd[0], FileEvent::READABLE, [&] { order += "R"; });

	concurrent::Semaphore sem(0);
	eq.add_ready_handler([&] {
		order += "P";
		sem.post();
	});

	std::thread other([&] {
		sem.wait();
		std::this_thread::sleep_for(8us);
		eq.pause_immediately();
	});

	eq.run();
	other.join();

	EXPECT_EQ(order, "P");
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, pause_immediately_before_run_is_noop) {
	EventQueue eq;
	string order;

	eq.pause_immediately();
	eq.add_ready_handler([&] { order += "x"; });
	eq.add_ready_handler([&] { order += "x"; });
	eq.pause_immediately();

	eq.run();
	EXPECT_EQ(order, "xx");
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, pause_immediately_and_run_again) {
	EventQueue eq;
	std::string order;

	eq.add_time_handler(1us, [&] {
		eq.pause_immediately();
		order += "1";
		eq.pause_immediately();
	});

	eq.add_time_handler(2us, [&] {
		eq.pause_immediately();
		order += "2";
		eq.pause_immediately();
	});

	eq.run();
	EXPECT_EQ(order, "1");
	eq.run();
	EXPECT_EQ(order, "12");
	eq.run();
	EXPECT_EQ(order, "12");
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, pause_immediately_and_run_again_loop) {
	EventQueue eq;
	std::string order;

	eq.add_repeating_handler(0ns, [&, iter = 0]() mutable {
		eq.pause_immediately();
		order += static_cast<char>(++iter + '0');
		return continue_repeating;
	});

	eq.run();
	EXPECT_EQ(order, "1");
	eq.run();
	EXPECT_EQ(order, "12");
	eq.run();
	EXPECT_EQ(order, "123");
	eq.run();
	EXPECT_EQ(order, "1234");
	eq.run();
	EXPECT_EQ(order, "12345");
	eq.run();
	EXPECT_EQ(order, "123456");
	eq.run();
	EXPECT_EQ(order, "1234567");
	eq.run();
	EXPECT_EQ(order, "12345678");
	eq.run();
	EXPECT_EQ(order, "123456789");
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, remove_time_handler) {
	auto start = system_clock::now();
	string order;

	EventQueue eq;
	auto hid = eq.add_time_handler(start + 3ms, [&] { FAIL(); });

	eq.add_time_handler(start + 2ms, [&] {
		EXPECT_LE(start + 2ms, system_clock::now());
		order += "2";
		eq.remove_handler(hid);

		eq.add_time_handler(start + 4ms, [&] {
			EXPECT_LE(start + 4ms, system_clock::now());
			order += "4";
		});

		eq.add_ready_handler([&] {
			EXPECT_LE(start + 2ms, system_clock::now());
			order += "r";
		});
	});

	eq.run();
	EXPECT_EQ(order, "2r4");
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue_DeathTest, time_handler_removes_itself) {
	EXPECT_EXIT(
	   {
		   EventQueue eq;
		   EventQueue::handler_id_t hid;
		   hid = eq.add_time_handler(system_clock::now(),
		                             [&] { eq.remove_handler(hid); });
		   eq.run();
	   },
	   ::testing::KilledBySignal(SIGABRT), "BUG");

	EXPECT_EXIT(
	   {
		   EventQueue eq;
		   EventQueue::handler_id_t hid;
		   hid = eq.add_time_handler(0ns, [&] { eq.remove_handler(hid); });
		   eq.run();
	   },
	   ::testing::KilledBySignal(SIGABRT), "BUG");
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue_DeathTest, ready_handler_removes_itself) {
	EXPECT_EXIT(
	   {
		   EventQueue eq;
		   EventQueue::handler_id_t hid;
		   hid = eq.add_ready_handler([&] { eq.remove_handler(hid); });
		   eq.run();
	   },
	   ::testing::KilledBySignal(SIGABRT), "BUG");
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, compaction_of_file_handlers_edge_case) {
	EventQueue eq;
	string order;

	FileDescriptor fd("/dev/null", O_WRONLY);
	EventQueue::handler_id_t hid1 = 0;
	EventQueue::handler_id_t hid2 = 0;
	hid1 = eq.add_file_handler(fd, FileEvent::WRITEABLE, [&] {
		eq.remove_handler(hid1);
		order += "1";

		hid2 = eq.add_file_handler(fd, FileEvent::WRITEABLE, [&] {
			// Now compaction removes the previous handler
			eq.remove_handler(hid2);
			order += "2";
			eq.pause_immediately();

			eq.add_file_handler(fd, FileEvent::WRITEABLE, [&] {
				order += "3";
				eq.pause_immediately();
			});
		});
	});

	eq.run();
	EXPECT_EQ(order, "12");
	eq.run();
	EXPECT_EQ(order, "123");
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, time_only_fairness) {
	auto start = system_clock::now();
	bool stop = false;

	EventQueue eq;
	eq.add_time_handler(start + 2ms, [&] {
		EXPECT_LE(start + 2ms, system_clock::now());
		stop = true;
	});

	uint64_t looper_iters = 0;
	auto looper = [&](auto&& self) {
		if (stop) {
			return;
		}

		++looper_iters;
		if (system_clock::now() > start + 100ms) {
			FAIL();
		}

		eq.add_ready_handler([&] { self(self); });
	};

	eq.add_ready_handler([&] { looper(looper); });

	eq.run();
	EXPECT_GT(looper_iters, 10);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, file_only_fairness) {
	uint64_t iters_a = 0;
	uint64_t iters_b = 0;
	EventQueue eq;

	FileDescriptor fd_a("/dev/zero", O_RDONLY);
	EventQueue::handler_id_t file_a_hid = eq.add_file_handler(
	   fd_a, FileEvent::READABLE, [&](FileEvent /*unused*/) {
		   if (iters_b + iters_b > 500) {
			   return eq.remove_handler(file_a_hid);
		   }

		   ++iters_a;
	   });

	FileDescriptor fd_b("/dev/null", O_WRONLY);
	EventQueue::handler_id_t file_b_hid = eq.add_file_handler(
	   fd_b, FileEvent::WRITEABLE, [&](FileEvent /*unused*/) {
		   if (iters_b + iters_b > 500) {
			   return eq.remove_handler(file_b_hid);
		   }

		   ++iters_b;
	   });

	eq.run();
	EXPECT_EQ(iters_a, iters_b);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, time_file_fairness) {
	auto start = system_clock::now();
	uint64_t looper_iters = 0;
	bool stop = false;

	EventQueue eq;
	function<void()> looper = [&] {
		if (stop) {
			return;
		}

		++looper_iters;
		if (system_clock::now() > start + 100ms) {
			FAIL();
		}

		eq.add_ready_handler(looper);
	};
	eq.add_ready_handler(looper);
	eq.add_time_handler(start + 2ms, [&] {
		EXPECT_LE(start + 2ms, system_clock::now());
		stop = true;
	});

	FileDescriptor fd("/dev/zero", O_RDONLY);
	int file_iters = 0;
	EventQueue::handler_id_t hid =
	   eq.add_file_handler(fd, FileEvent::READABLE, [&](FileEvent /*unused*/) {
		   if (stop) {
			   return eq.remove_handler(hid);
		   }

		   if (system_clock::now() > start + 100ms) {
			   FAIL();
			   return eq.remove_handler(hid);
		   }

		   ++file_iters;
	   });

	eq.run();
	EXPECT_GT(looper_iters, 10);
	EXPECT_GT(file_iters, 4);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, full_fairness) {
	auto start = system_clock::now();
	uint64_t iters_a = 0;
	uint64_t iters_b = 0;
	bool stop = false;
	EventQueue eq;

	FileDescriptor fd_a("/dev/zero", O_RDONLY);
	EventQueue::handler_id_t file_a_hid = eq.add_file_handler(
	   fd_a, FileEvent::READABLE, [&](FileEvent /*unused*/) {
		   if (stop) {
			   return eq.remove_handler(file_a_hid);
		   }

		   if (system_clock::now() > start + 100ms) {
			   FAIL();
			   return eq.remove_handler(file_a_hid);
		   }

		   ++iters_a;
	   });

	FileDescriptor fd_b("/dev/null", O_WRONLY);
	EventQueue::handler_id_t file_b_hid = eq.add_file_handler(
	   fd_b, FileEvent::WRITEABLE, [&](FileEvent /*unused*/) {
		   if (stop) {
			   return eq.remove_handler(file_b_hid);
		   }

		   if (system_clock::now() > start + 100ms) {
			   FAIL();
			   return eq.remove_handler(file_b_hid);
		   }

		   ++iters_b;
	   });

	uint looper_iters = 0;
	function<void()> looper = [&] {
		if (stop) {
			return;
		}

		++looper_iters;
		if (system_clock::now() > start + 100ms) {
			FAIL();
		}

		eq.add_ready_handler(looper);
	};
	eq.add_ready_handler(looper);
	eq.add_time_handler(start + 2ms, [&] {
		EXPECT_LE(start + 2ms, system_clock::now());
		stop = true;
	});

	eq.run();
	EXPECT_GT(looper_iters, 10);
	EXPECT_GT(iters_a, 10);
	EXPECT_GT(iters_b, 10);
	EXPECT_NEAR(iters_a, iters_b, 1);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, file_unready_read_and_close_event) {
	std::array<int, 2> pfd{};
	ASSERT_EQ(pipe2(pfd.data(), O_NONBLOCK | O_CLOEXEC), 0);
	FileDescriptor rfd(pfd[0]);
	FileDescriptor wfd(pfd[1]);
	std::string buff(10, '\0');

	EventQueue eq;
	auto start = system_clock::now();
	eq.add_time_handler(start + 2ms,
	                    [&] { write(wfd, "Test", sizeof("Test")); });
	eq.add_time_handler(start + 3ms, [&] { (void)wfd.close(); });

	EXPECT_EQ(read(rfd, buff.data(), buff.size()), -1);
	EXPECT_EQ(errno, EAGAIN);

	int round = 0;
	std::array expected_events = {FileEvent::READABLE, FileEvent::CLOSED};
	EventQueue::handler_id_t hid =
	   eq.add_file_handler(rfd, FileEvent::READABLE, [&](FileEvent events) {
		   EXPECT_EQ(events, expected_events[round++]);
		   if (events == FileEvent::CLOSED) {
			   return eq.remove_handler(hid);
		   }

		   int rc = read(rfd, buff.data(), buff.size());
		   ASSERT_EQ(rc, sizeof("Test"));
		   buff.resize(rc - 1);
		   ASSERT_EQ(buff, "Test");
	   });

	eq.run();
	EXPECT_EQ(round, 2);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, file_simultaneous_read_and_close_event) {
	std::array<int, 2> pfd{};
	ASSERT_EQ(pipe2(pfd.data(), O_NONBLOCK | O_CLOEXEC), 0);
	FileDescriptor rfd(pfd[0]);
	FileDescriptor wfd(pfd[1]);
	write(wfd, "Test", sizeof("Test"));
	(void)wfd.close();

	EventQueue eq;
	EventQueue::handler_id_t hid =
	   eq.add_file_handler(rfd, FileEvent::READABLE, [&](FileEvent events) {
		   EXPECT_EQ(events, FileEvent::READABLE | FileEvent::CLOSED);
		   eq.remove_handler(hid);
	   });

	eq.run();
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueueDeathTest, time_handler_removing_itself) {
	auto start = system_clock::now();
	EventQueue eq;
	int iters = 0;
	EventQueue::handler_id_t hid = eq.add_time_handler(start + 1ms, [&] {
		++iters;
		ASSERT_DEATH({ eq.remove_handler(hid); }, "");
	});

	eq.run();
	EXPECT_EQ(iters, 1);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, file_handler_removing_itself) {
	FileDescriptor fd("/dev/null", O_RDONLY);
	EventQueue eq;
	int iters = 0;
	EventQueue::handler_id_t hid =
	   eq.add_file_handler(fd, FileEvent::READABLE, [&](FileEvent /*unused*/) {
		   ++iters;
		   eq.remove_handler(hid);
	   });

	eq.run();
	EXPECT_EQ(iters, 1);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, time_handler_removing_file_handler_simple) {
	FileDescriptor fd("/dev/null", O_RDONLY);
	EventQueue eq;
	int iters = 0;
	auto hid = eq.add_file_handler(fd, FileEvent::READABLE, [&] { ++iters; });

	eq.add_time_handler(system_clock::now() + 1ms,
	                    [&] { eq.remove_handler(hid); });

	eq.run();
	EXPECT_GT(iters, 10);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue,
     time_handler_removing_file_handler_while_processing_file_handlers) {
	FileDescriptor fd("/dev/null", O_RDONLY);
	EventQueue eq;
	int iters_a = 0;
	int iters_b = 0;
	EventQueue::handler_id_t hid_a = 0;
	EventQueue ::handler_id_t hid_b = 0;
	hid_a = eq.add_file_handler(fd, FileEvent::READABLE, [&] {
		++iters_a;
		eq.add_ready_handler([&] {
			EXPECT_EQ(iters_b, 0);
			eq.remove_handler(hid_b);
		});

		std::this_thread::sleep_for(1ms);
		eq.remove_handler(hid_a);
	});

	hid_b = eq.add_file_handler(fd, FileEvent::READABLE, [&] {
		++iters_b;
		eq.remove_handler(hid_b); // In case of error
	});

	eq.run();
	EXPECT_EQ(iters_a, 1);
	EXPECT_EQ(iters_b, 0);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, file_handler_removing_other_file_handler) {
	FileDescriptor fd("/dev/null", O_RDONLY);
	EventQueue::handler_id_t hid_a = 0;
	EventQueue::handler_id_t hid_b = 0;
	EventQueue::handler_id_t hid_c = 0;
	EventQueue::handler_id_t hid_d = 0;
	int iters_a = 0;
	int iters_b = 0;
	int iters_c = 0;
	int iters_d = 0;
	EventQueue eq;
	hid_a = eq.add_file_handler(fd, FileEvent::READABLE, [&] {
		++iters_a;
		if (iters_a == 1) {
			eq.remove_handler(hid_b);
		}
	});

	hid_b = eq.add_file_handler(fd, FileEvent::READABLE, [&] {
		++iters_b;
		assert(iters_b < 1000); // In case of error
	});

	hid_c = eq.add_file_handler(fd, FileEvent::READABLE, [&] {
		++iters_c;
		assert(iters_c < 1000); // In case of error
	});

	hid_d = eq.add_file_handler(fd, FileEvent::READABLE, [&] {
		++iters_d;
		if (iters_d == 1) {
			eq.remove_handler(hid_c);
		}
	});

	eq.add_time_handler(system_clock::now() + 1ms, [&] {
		eq.remove_handler(hid_a);
		eq.remove_handler(hid_d);
	});

	eq.run();
	EXPECT_GT(iters_a, 4);
	EXPECT_EQ(iters_b, 0);
	EXPECT_EQ(iters_c, 1);
	EXPECT_GT(iters_d, 4);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, test_adding_new_file_handlers_after_removing_old_ones) {
	FileDescriptor fd("/dev/null", O_RDONLY);
	vector<EventQueue::handler_id_t> hids;
	vector<int> iters;

	EventQueue eq;
	for (int i = 0; i < 100; ++i) {
		constexpr auto handlers_to_add = 10;
		eq.add_ready_handler([&] {
			for (int j = 0; j < handlers_to_add; ++j) {
				auto hid =
				   eq.add_file_handler(fd, FileEvent::READABLE,
				                       [&eq, &iters, &hids, idx = hids.size()] {
					                       ++iters[idx];
					                       eq.remove_handler(hids[idx]);
				                       });

				hids.emplace_back(hid);
				iters.emplace_back(0);
			}
		});
	}

	eq.run();
	assert(size(iters) == size(hids));
	for (size_t i = 0; i < size(iters); ++i) {
		EXPECT_EQ(iters[i], 1) << "i: " << i;
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(EventQueue, move_constructor_and_move_operator) {
	EventQueue eq;
	string order;
	eq.add_repeating_handler(100us, [&, iter = 0]() mutable {
		order += '1';
		++iter;
		return iter == 5 ? stop_repeating : continue_repeating;
	});
	eq.add_repeating_handler(100us, [&, iter = 0]() mutable {
		order += '2';
		++iter;
		if (iter == 2) {
			order += 'S';
			eq.pause_immediately();
			eq.add_ready_handler([&] { order += 'r'; });
		}
		return iter == 3 ? stop_repeating : continue_repeating;
	});

	int times_pipe_was_read = 0;
	std::array<int, 2> pfd{};
	if (pipe2(pfd.data(), O_CLOEXEC)) {
		THROW("pipe2()", errmsg());
	}
	Defer guard = [&pfd] {
		for (int fd : pfd) {
			(void)close(fd);
		}
	};
	// File handler that will read twice: before and after pause
	EventQueue::handler_id_t fhid = 0;
	fhid = eq.add_file_handler(pfd[0], FileEvent::READABLE, [&]() {
		std::array<char, 2> buf{};
		int rc = read(pfd[0], buf.data(), buf.size());
		assert(rc == 1);
		if (++times_pipe_was_read == 2) {
			eq.remove_handler(fhid);
		}
	});
	int rc = write(pfd[1], "x", 1);
	assert(rc == 1);

	eq.run();
	EXPECT_EQ(times_pipe_was_read, 1);

	EventQueue other = std::move(eq);
	other.add_ready_handler([&] { order += 'r'; });
	eq = std::move(other);

	rc = write(pfd[1], "y", 1);
	assert(rc == 1);
	eq.run();
	EXPECT_EQ(times_pipe_was_read, 2);
	EXPECT_EQ(order, "1212Srr1211");
}
