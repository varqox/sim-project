#include "simlib/inotify.hh"
#include "simlib/concurrent/semaphore.hh"
#include "simlib/file_contents.hh"
#include "simlib/file_manip.hh"
#include "simlib/temporary_directory.hh"
#include "simlib/temporary_file.hh"

#include <gtest/gtest.h>

using std::string;
using std::vector;
using VS = vector<string>;
using namespace std::chrono_literals;

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(inotify_FileModificationMonitor /*unused*/, simple /*unused*/) {
	TemporaryDirectory tmp_dir("/tmp/inotify_test.XXXXXX");
	TemporaryFile tmp_file("/tmp/inotify_test.XXXXXX");
	FileModificationMonitor monitor;
	VS order;

	monitor.add_path(tmp_dir.path(), 50us);
	monitor.add_path(tmp_file.path(), 50us);

	monitor.set_event_handler(
	   [&](const string& path) { order.emplace_back(path); });

	VS paths = {
	   concat_tostr(tmp_dir.path(), "abc"),
	   concat_tostr(tmp_dir.path(), "efg"),
	   concat_tostr(tmp_dir.path(), "dirdir"),
	   tmp_file.path(),
	   concat_tostr(tmp_dir.path(), "abc"),
	};

	monitor.event_queue().add_repeating_handler(100us, [&, i = 0U]() mutable {
		order.emplace_back(concat_tostr(i));
		if (has_suffix(paths[i], "dirdir")) {
			EXPECT_EQ(mkdir(paths[i].data(), S_0755), 0);
		} else {
			put_file_contents(paths[i], "abc");
		}

		if (++i == paths.size()) {
			monitor.event_queue().add_time_handler(
			   200us, [&] { monitor.pause_immediately(); });
			return stop_repeating;
		}
		return continue_repeating;
	});

	monitor.watch();
	EXPECT_EQ(order, (VS{
	                    "0",
	                    paths[0],
	                    "1",
	                    paths[1],
	                    "2",
	                    paths[2],
	                    "3",
	                    paths[3],
	                    "4",
	                    paths[4],
	                 }));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(inotify_FileModificationMonitor /*unused*/,
     remove_is_not_an_event /*unused*/) {
	TemporaryDirectory tmp_dir("/tmp/inotify_test.XXXXXX");
	TemporaryFile tmp_file("/tmp/inotify_test.XXXXXX");
	FileModificationMonitor monitor;
	VS order;

	monitor.add_path(tmp_dir.path());
	monitor.add_path(tmp_file.path());

	monitor.set_event_handler(
	   [&](const string& path) { order.emplace_back(path); });

	auto dir_abc_path = concat_tostr(tmp_dir.path(), "abc");
	auto dir_dirdir_path = concat_tostr(tmp_dir.path(), "dirdir");
	auto dir_efg_path = concat_tostr(tmp_dir.path(), "efg");

	monitor.event_queue().add_time_handler(100us, [&] {
		order.emplace_back("+abc");
		put_file_contents(dir_abc_path, "");
		monitor.event_queue().add_time_handler(100us, [&] {
			order.emplace_back("+dirdir");
			EXPECT_EQ(mkdir(dir_dirdir_path.data(), S_0755), 0);
			monitor.event_queue().add_time_handler(100us, [&] {
				order.emplace_back("+tmpfile");
				put_file_contents(tmp_file.path(), "");
				monitor.event_queue().add_time_handler(100us, [&] {
					order.emplace_back("--");
					EXPECT_EQ(remove(dir_abc_path), 0);
					EXPECT_EQ(remove(dir_dirdir_path), 0);
					EXPECT_EQ(remove(tmp_file.path()), 0);
					monitor.event_queue().add_time_handler(100us, [&] {
						order.emplace_back("+efg");
						put_file_contents(dir_efg_path, "");
						monitor.event_queue().add_time_handler(100us, [&] {
							order.emplace_back("stop");
							monitor.pause_immediately();
						});
					});
				});
			});
		});
	});

	monitor.watch();
	EXPECT_EQ(order, (VS{
	                    "+abc",
	                    dir_abc_path,
	                    "+dirdir",
	                    dir_dirdir_path,
	                    "+tmpfile",
	                    tmp_file.path(),
	                    "--",
	                    "+efg",
	                    dir_efg_path,
	                    "stop",
	                 }));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(inotify_FileModificationMonitor /*unused*/,
     stillness_threshold /*unused*/) {
	TemporaryDirectory tmp_dir("/tmp/inotify_test.XXXXXX");
	TemporaryFile tmp_file("/tmp/inotify_test.XXXXXX");
	FileModificationMonitor monitor;
	VS order;

	monitor.add_path(tmp_dir.path());
	monitor.add_path(tmp_file.path(), 300us);

	monitor.set_event_handler(
	   [&](const string& path) { order.emplace_back(path); });

	auto dir_abc_path = concat_tostr(tmp_dir.path(), "abc");
	put_file_contents(dir_abc_path, ""); // create file

	monitor.event_queue().add_repeating_handler(100us, [&, i = 0U]() mutable {
		order.emplace_back("write");
		put_file_contents(dir_abc_path, "xxx");
		put_file_contents(tmp_file.path(), "xxx");

		if (++i == 3) {
			monitor.event_queue().add_time_handler(
			   400us, [&] { monitor.pause_immediately(); });
			return stop_repeating;
		}
		return continue_repeating;
	});

	monitor.watch();
	EXPECT_EQ(order, (VS{
	                    "write",
	                    dir_abc_path,
	                    "write",
	                    dir_abc_path,
	                    "write",
	                    dir_abc_path,
	                    tmp_file.path(),
	                 }));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(inotify_FileModificationMonitor /*unused*/,
     rewatch_after_remove /*unused*/) {
	TemporaryDirectory tmp_dir("/tmp/inotify_test.XXXXXX");
	TemporaryFile tmp_file("/tmp/inotify_test.XXXXXX");
	FileModificationMonitor monitor;
	VS order;

	monitor.add_path(tmp_dir.path());
	monitor.add_path(tmp_file.path());

	monitor.set_event_handler(
	   [&](const string& path) { order.emplace_back(path); });

	auto dir_abc_path = concat_tostr(tmp_dir.path(), "abc");

	monitor.event_queue().add_time_handler(100us, [&] {
		order.emplace_back("+");
		put_file_contents(tmp_file.path(), "");
		monitor.event_queue().add_time_handler(100us, [&] {
			order.emplace_back("+");
			put_file_contents(dir_abc_path, "");
			monitor.event_queue().add_time_handler(100us, [&] {
				order.emplace_back("--");
				EXPECT_EQ(remove_r(tmp_dir.path()), 0);
				EXPECT_EQ(remove(tmp_file.path()), 0);
				monitor.event_queue().add_time_handler(100us, [&] {
					order.emplace_back("*");
					EXPECT_EQ(mkdir(tmp_dir.path()), 0);
					monitor.event_queue().add_time_handler(1100us, [&] {
						order.emplace_back("*");
						put_file_contents(tmp_file.path(), "");
						monitor.event_queue().add_time_handler(
						   1100us, [&] { monitor.pause_immediately(); });
					});
				});
			});
		});
	});

	monitor.set_add_missing_files_retry_period(1ms);
	monitor.watch();
	EXPECT_EQ(order, (VS{
	                    "+",
	                    tmp_file.path(),
	                    "+",
	                    dir_abc_path,
	                    "--",
	                    "*",
	                    tmp_dir.path(),
	                    "*",
	                    tmp_file.path(),
	                 }));
}
