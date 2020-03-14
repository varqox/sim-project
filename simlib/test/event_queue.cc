#include "../include/event_queue.hh"
#include "../include/file_descriptor.hh"

#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <thread>
#include <unistd.h>

using std::array;
using std::chrono::milliseconds;
using std::chrono::system_clock;
using std::chrono_literals::operator""ms;
using std::chrono_literals::operator""us;
using std::chrono_literals::operator""ns;
using std::function;
using std::string;

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

TEST(EventQueue, add_repeating_handler) {
	auto start = system_clock::now();
	string order;

	EventQueue eq;
	eq.add_repeating_handler(100us, [&, iter = 0]() mutable {
		++iter;
		order += char('a' + iter);

		EXPECT_LE(start + iter * 100us, system_clock::now());
		return (iter != 10);
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
	EXPECT_EQ(order, "1b2cdefghijk");
}

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
		if (stop)
			return;

		++looper_iters;
		if (system_clock::now() > start + 100ms)
			FAIL();

		eq.add_ready_handler([&] { self(self); });
	};

	eq.add_ready_handler([&] { looper(looper); });

	eq.run();
	EXPECT_GT(looper_iters, 10);
}

TEST(EventQueue, file_only_fairness) {
	uint64_t iters_a = 0, iters_b = 0;
	EventQueue eq;

	FileDescriptor fd_a("/dev/zero", O_RDONLY);
	EventQueue::handler_id_t file_a_hid =
	   eq.add_file_handler(fd_a, FileEvent::READABLE, [&](FileEvent) {
		   if (iters_b + iters_b > 500)
			   return eq.remove_handler(file_a_hid);

		   ++iters_a;
	   });

	FileDescriptor fd_b("/dev/null", O_WRONLY);
	EventQueue::handler_id_t file_b_hid =
	   eq.add_file_handler(fd_b, FileEvent::WRITEABLE, [&](FileEvent) {
		   if (iters_b + iters_b > 500)
			   return eq.remove_handler(file_b_hid);

		   ++iters_b;
	   });

	eq.run();
	EXPECT_EQ(iters_a, iters_b);
}

TEST(EventQueue, time_file_fairness) {
	auto start = system_clock::now();
	uint64_t looper_iters = 0;
	bool stop = false;

	EventQueue eq;
	function<void()> looper = [&] {
		if (stop)
			return;

		++looper_iters;
		if (system_clock::now() > start + 100ms)
			FAIL();

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
	   eq.add_file_handler(fd, FileEvent::READABLE, [&](FileEvent) {
		   if (stop)
			   return eq.remove_handler(hid);

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

TEST(EventQueue, full_fairness) {
	auto start = system_clock::now();
	uint64_t iters_a = 0, iters_b = 0;
	bool stop = false;
	EventQueue eq;

	FileDescriptor fd_a("/dev/zero", O_RDONLY);
	EventQueue::handler_id_t file_a_hid =
	   eq.add_file_handler(fd_a, FileEvent::READABLE, [&](FileEvent) {
		   if (stop)
			   return eq.remove_handler(file_a_hid);

		   if (system_clock::now() > start + 100ms) {
			   FAIL();
			   return eq.remove_handler(file_a_hid);
		   }

		   ++iters_a;
	   });

	FileDescriptor fd_b("/dev/null", O_WRONLY);
	EventQueue::handler_id_t file_b_hid =
	   eq.add_file_handler(fd_b, FileEvent::WRITEABLE, [&](FileEvent) {
		   if (stop)
			   return eq.remove_handler(file_b_hid);

		   if (system_clock::now() > start + 100ms) {
			   FAIL();
			   return eq.remove_handler(file_b_hid);
		   }

		   ++iters_b;
	   });

	uint looper_iters = 0;
	function<void()> looper = [&] {
		if (stop)
			return;

		++looper_iters;
		if (system_clock::now() > start + 100ms)
			FAIL();

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

TEST(EventQueue, file_unready_read_and_close_event) {
	std::array<int, 2> pfd;
	ASSERT_EQ(pipe2(pfd.data(), O_NONBLOCK), 0);
	FileDescriptor rfd(pfd[0]), wfd(pfd[1]);
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
		   if (events == FileEvent::CLOSED)
			   return eq.remove_handler(hid);

		   int rc = read(rfd, buff.data(), buff.size());
		   ASSERT_EQ(rc, sizeof("Test"));
		   buff.resize(rc - 1);
		   ASSERT_EQ(buff, "Test");
	   });

	eq.run();
	EXPECT_EQ(round, 2);
}

TEST(EventQueue, file_simultaneous_read_and_close_event) {
	std::array<int, 2> pfd;
	ASSERT_EQ(pipe2(pfd.data(), O_NONBLOCK), 0);
	FileDescriptor rfd(pfd[0]), wfd(pfd[1]);
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

TEST(EventQueue, file_handler_removing_itself) {
	FileDescriptor fd("/dev/null", O_RDONLY);
	EventQueue eq;
	int iters = 0;
	EventQueue::handler_id_t hid =
	   eq.add_file_handler(fd, FileEvent::READABLE, [&](FileEvent) {
		   ++iters;
		   eq.remove_handler(hid);
	   });

	eq.run();
	EXPECT_EQ(iters, 1);
}

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

TEST(EventQueue,
     time_handler_removing_file_handler_while_processing_file_handlers) {
	FileDescriptor fd("/dev/null", O_RDONLY);
	EventQueue eq;
	int iters_a = 0, iters_b = 0;
	EventQueue::handler_id_t hid_a, hid_b;
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

TEST(EventQueue, file_handler_removing_other_file_handler) {
	FileDescriptor fd("/dev/null", O_RDONLY);
	EventQueue::handler_id_t hid_a, hid_b, hid_c, hid_d;
	int iters_a = 0, iters_b = 0, iters_c = 0, iters_d = 0;
	EventQueue eq;
	hid_a = eq.add_file_handler(fd, FileEvent::READABLE, [&] {
		++iters_a;
		if (iters_a == 1)
			eq.remove_handler(hid_b);
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
		if (iters_d == 1)
			eq.remove_handler(hid_c);
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

TEST(EventQueue, test_adding_new_file_handlers_after_removing_old_ones) {
	FileDescriptor fd("/dev/null", O_RDONLY);
	std::vector<EventQueue::handler_id_t> hids;
	std::vector<int> iters;

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
	for (size_t i = 0; i < size(iters); ++i)
		EXPECT_EQ(iters[i], 1) << "i: " << i;
}
