#include "simlib/inotify.hh"
#include "simlib/concurrent/semaphore.hh"
#include "simlib/file_contents.hh"
#include "simlib/file_manip.hh"
#include "simlib/random.hh"
#include "simlib/repeating.hh"
#include "simlib/temporary_directory.hh"
#include "simlib/temporary_file.hh"

#include <chrono>
#include <cstdio>
#include <gtest/gtest.h>
#include <map>
#include <string>

using std::multimap;
using std::string;
using namespace std::chrono_literals;
using std::chrono::nanoseconds;
using std::chrono::system_clock;

struct InotifyTest {
    TemporaryDirectory dir{"/tmp/inotify_test.XXXXXX"};
    TemporaryFile file{"/tmp/inotify_test.XXXXXX"};
    FileModificationMonitor monitor;
    multimap<std::string, system_clock::time_point> fired_files;
    nanoseconds stillness_threhold = get_random(0, 100) * 1us;
    bool no_more_events = false;
    bool checked = false;

    InotifyTest() = default;
    InotifyTest(const InotifyTest&) = delete;
    InotifyTest(InotifyTest&&) = delete;
    InotifyTest& operator=(const InotifyTest&) = delete;
    InotifyTest& operator=(InotifyTest&&) = delete;

    ~InotifyTest() { assert(checked); }

    void init() {
        monitor.add_path(dir.path(), stillness_threhold);
        monitor.add_path(file.path(), stillness_threhold);
        monitor.set_event_handler([&](const string& path) {
            handle(path);
            if (no_more_events and fired_files.empty()) {
                monitor.event_queue().add_time_handler(
                    50us, [&] { monitor.pause_immediately(); });
            }
        });
    }

    void init_and_watch() {
        init();
        monitor.watch();
    }

    void fire(string path, bool last = false) {
        fired_files.emplace(std::move(path), system_clock::now());
        if (last) {
            ASSERT_FALSE(no_more_events);
            no_more_events = true;
        }
    }

    void handle(const string& path) {
        auto [beg, end] = fired_files.equal_range(path);
        ASSERT_NE(beg, end) << "unexpected events on file " << path;
        // .emplace() always inserts at the upper bound, so times for the same
        // file are chronological
        EXPECT_LE(beg->second + stillness_threhold, system_clock::now());
        fired_files.erase(beg);
    }

    void check() {
        EXPECT_TRUE(fired_files.empty());
        checked = true;
    }
};

// NOLINTNEXTLINE
TEST(inotify_FileModificationMonitor, creating_file_in_dir) {
    InotifyTest it;

    auto abc_path = it.dir.path() + "abc";
    it.monitor.event_queue().add_ready_handler([&] {
        ASSERT_EQ(create_file(abc_path), 0);
        it.fire(abc_path, true);
    });

    it.init_and_watch();
    it.check();
}

// NOLINTNEXTLINE
TEST(inotify_FileModificationMonitor, creating_dir_in_dir) {
    InotifyTest it;

    auto abc_path = it.dir.path() + "abc";
    it.monitor.event_queue().add_ready_handler([&] {
        ASSERT_EQ(mkdir(abc_path), 0);
        it.fire(abc_path, true);
    });

    it.init_and_watch();
    it.check();
}

// NOLINTNEXTLINE
TEST(inotify_FileModificationMonitor, modify_file) {
    InotifyTest it;

    auto abc_path = it.dir.path() + "abc";
    ASSERT_EQ(create_file(abc_path), 0);

    it.monitor.event_queue().add_ready_handler([&] {
        put_file_contents(it.file.path(), "x");
        it.fire(it.file.path());

        put_file_contents(abc_path, "x");
        it.fire(abc_path, true);
    });

    it.init_and_watch();
    it.check();
}

// NOLINTNEXTLINE
TEST(inotify_FileModificationMonitor, create_watched_file_or_dir) {
    InotifyTest it;
    it.monitor.set_add_missing_files_retry_period(get_random(0, 100) * 1us);

    ASSERT_EQ(remove(it.dir.path()), 0);
    ASSERT_EQ(remove(it.file.path()), 0);

    it.monitor.event_queue().add_ready_handler([&] {
        ASSERT_EQ(mkdir(it.dir.path()), 0);
        it.fire(it.dir.path());
        ASSERT_EQ(create_file(it.file.path()), 0);
        it.fire(it.file.path(), true);
    });

    it.init_and_watch();
    it.check();
}

// NOLINTNEXTLINE
TEST(inotify_FileModificationMonitor, remove_is_not_an_event) {
    InotifyTest it;

    auto abc_path = it.dir.path() + "abc";
    ASSERT_EQ(create_file(abc_path), 0);
    auto efg_path = it.dir.path() + "efg/";
    ASSERT_EQ(mkdir(efg_path), 0);
    auto hij_path = it.dir.path() + "hij";

    it.monitor.event_queue().add_ready_handler([&] {
        ASSERT_EQ(remove(it.file.path()), 0);
        ASSERT_EQ(remove(abc_path), 0);
        ASSERT_EQ(remove(efg_path), 0);
        ASSERT_EQ(create_file(hij_path), 0);
        it.fire(hij_path, true);
    });

    it.init_and_watch();
    it.check();
}

// NOLINTNEXTLINE
TEST(inotify_FileModificationMonitor, rewatch_after_remove) {
    InotifyTest it;
    it.monitor.set_add_missing_files_retry_period(get_random(0, 100) * 1us);

    it.monitor.event_queue().add_ready_handler([&] {
        ASSERT_EQ(remove(it.file.path()), 0);
        ASSERT_EQ(remove(it.dir.path()), 0);
        it.monitor.event_queue().add_time_handler(get_random(0, 100) * 1us, [&] {
            ASSERT_EQ(mkdir(it.dir.path()), 0);
            it.fire(it.dir.path());
            ASSERT_EQ(create_file(it.file.path()), 0);
            it.fire(it.file.path(), true);
        });
    });

    it.init_and_watch();
    it.check();
}

// NOLINTNEXTLINE
TEST(inotify_FileModificationMonitor, event_in_pause) {
    InotifyTest it;

    auto abc_path = it.dir.path() + "abc";
    it.monitor.event_queue().add_ready_handler([&] {
        put_file_contents(it.file.path(), "x");
        it.fire(it.file.path());
        it.monitor.pause_immediately();
    });

    it.init_and_watch();
    ASSERT_EQ(create_file(abc_path), 0);
    it.fire(abc_path, true);

    it.monitor.watch();
    it.check();
}
