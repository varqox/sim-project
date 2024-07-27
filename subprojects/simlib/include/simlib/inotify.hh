#pragma once

#include <chrono>
#include <linux/limits.h>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <simlib/errmsg.hh>
#include <simlib/event_queue.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/logger.hh>
#include <simlib/string_view.hh>
#include <sys/inotify.h>

class WatchingLog {
public:
    WatchingLog() = default;
    WatchingLog(const WatchingLog&) = default;
    WatchingLog(WatchingLog&&) = default;
    WatchingLog& operator=(const WatchingLog&) = default;
    WatchingLog& operator=(WatchingLog&&) = default;

    virtual void could_not_watch(const std::string& path, int errnum) = 0;
    virtual void read_failed(int errnum) = 0;
    virtual void started_watching(const std::string& file) = 0;
    virtual ~WatchingLog() = default;
};

class SilentWatchingLog : public WatchingLog {
public:
    void could_not_watch(const std::string& /*path*/, int /*errnum*/) override {}

    void read_failed(int /*errnum*/) override {}

    void started_watching(const std::string& /*file*/) override {}
};

class SimpleWatchingLog : public WatchingLog {
public:
    void could_not_watch(const std::string& path, int errnum) override {
        stdlog("warning: could not watch ", path, ": inotify_add_watch()", errmsg(errnum));
    }

    void read_failed(int errnum) override { stdlog("warning: read()", errmsg(errnum)); }

    void started_watching(const std::string& file) override { stdlog("Started watching: ", file); }
};

class FileModificationMonitor {
    using milliseconds = std::chrono::milliseconds;
    using nanoseconds = std::chrono::nanoseconds;
    using time_point = std::chrono::system_clock::time_point;

    struct FileInfo {
        std::string path;
        nanoseconds stillness_threshold;
        bool notify_if_attribute_changes;

        friend bool operator<(const FileInfo& a, const FileInfo& b) noexcept {
            return a.path < b.path;
        }
    };

    std::set<FileInfo> added_files_;

    std::set<const FileInfo*> unwatched_files_;
    std::map<int, const FileInfo*> watched_files_; // wd => FileInfo
    bool processing_unwatched_files_is_scheduled_ = false;

    static constexpr auto events_requiring_handler = IN_MODIFY | // modification
        IN_CREATE | IN_MOVED_TO | // file creation
        IN_ATTRIB; // attribute change

    static constexpr auto all_events = events_requiring_handler | IN_MOVE_SELF | // deletion
        IN_EXCL_UNLINK; // do not monitor unlinked files

    struct simlib_inotify_event {
        decltype(inotify_event::wd) wd{};
        decltype(inotify_event::mask) mask{};
        decltype(inotify_event::cookie) cookie{};
        std::optional<CStringView> file_name;
    };

    EventQueue eq_;
    FileDescriptor intfy_fd_;
    std::unique_ptr<char[]> intfy_buff_;
    std::map<std::string, EventQueue::handler_id_t> deferred_modification_handlers_;
    std::unique_ptr<WatchingLog> watching_log_ = std::make_unique<SilentWatchingLog>();
    std::function<void(const std::string&)> event_handler_;
    nanoseconds add_missing_files_retry_period_ = milliseconds(50);

public:
    /**
     * @brief Adds @p path to a file or directory to watch
     *
     * @param path path to a file or directory to watch (it is not recursive)
     * @param stillness_threshold the event handler will be called just after
     *   @p stillness_threshold time duration without modification events
     *   happening (it is useful, as saving file often takes several writes and
     *   you would want one event for that)
     */
    void add_path(
        std::string path,
        bool notify_if_attribute_changes = false,
        nanoseconds stillness_threshold = nanoseconds(0)
    ) {
        STACK_UNWINDING_MARK;

        assert(stillness_threshold >= nanoseconds(0));
        auto [it, inserted] = added_files_.insert({
            .path = std::move(path),
            .stillness_threshold = stillness_threshold,
            .notify_if_attribute_changes = notify_if_attribute_changes,
        });
        assert(inserted and "Path added more than once");
        try {
            unwatched_files_.emplace(&*it);
            schedule_processing_unwatched_files(); // Needed in case the event loop is now running.
        } catch (...) {
            added_files_.erase(it);
            throw;
        }
    }

    EventQueue& event_queue() noexcept { return eq_; }

    // @param watching_log logger for warnings and start-watching events
    void set_watching_log(std::unique_ptr<WatchingLog> watching_log) {
        watching_log_ = std::move(watching_log);
    }

    /**
     * @brief Installs @p event_handler that will be called on every
     *   modification / creation (excluding rename or deletion) event on watched
     *   files. It should accept const std::string& as an argument that denotes
     *   a path of the file correlated with this event.
     */
    void set_event_handler(std::function<void(const std::string&)> event_handler) {
        event_handler_ = std::move(event_handler);
    }

    void set_add_missing_files_retry_period(nanoseconds val) {
        add_missing_files_retry_period_ = val;
    }

    /**
     * @brief Watches files and directories specified via add_path() and calls set event handler
     *   for every creation / modification (excluding rename and deletion) event on watched files or
     *   files directly inside watched directories.
     *   This function starts watching the added files before running the event queue.
     *
     * @errors will be thrown as std::runtime_exception with an appropriate
     *   message.
     */
    void watch() {
        STACK_UNWINDING_MARK;
        if (not intfy_fd_.is_open()) {
            init_watching();
        }
        eq_.run();
    }

    // Stops processing of events immediately. It is safe to call it from other
    // threads.
    void pause_immediately() noexcept { return eq_.pause_immediately(); }

private:
    void init_watching();

    void process_event(const simlib_inotify_event& event);

    void schedule_processing_unwatched_files();

    void process_unwatched_files(bool run_modification_handler_on_success);

    template <class String>
    void run_modification_handler(const FileInfo* finfo, String&& file_path);
};
