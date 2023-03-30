#include <simlib/inotify.hh>
#include <sys/inotify.h>

void FileModificationMonitor::init_watching() {
    intfy_fd_ = inotify_init1(IN_CLOEXEC);
    if (not intfy_fd_.is_open()) {
        THROW("inotify_init1()", errmsg());
    }

    constexpr size_t intfy_buff_len = (sizeof(inotify_event) + PATH_MAX + 1) * 16;
    intfy_buff_ = std::make_unique<char[]>(intfy_buff_len);
    eq_.add_file_handler(intfy_fd_, FileEvent::READABLE, [&] {
        ssize_t read_len = read(intfy_fd_, intfy_buff_.get(), intfy_buff_len);
        if (read_len < static_cast<ssize_t>(sizeof(inotify_event))) {
            watching_log_->read_failed(errno);
            return;
        }

        char* ptr = intfy_buff_.get();
        while (ptr < intfy_buff_.get() + read_len) {
            simlib_inotify_event event;
            std::memcpy(&event.wd, ptr + offsetof(inotify_event, wd), sizeof(event.wd));
            std::memcpy(&event.mask, ptr + offsetof(inotify_event, mask), sizeof(event.mask));
            std::memcpy(&event.cookie, ptr + offsetof(inotify_event, cookie), sizeof(event.cookie));
            // event.file_name
            decltype(inotify_event::len) len = 0;
            std::memcpy(&len, ptr + offsetof(inotify_event, len), sizeof(len));
            if (len > 0) {
                event.file_name = CStringView(ptr + offsetof(inotify_event, name));
            }
            ptr += sizeof(inotify_event) + len;

            process_event(event);
        }
    });

    process_unwatched_files(false);
    // If same files couldn't be added, we will try to add them repeatedly
    schedule_processing_unwatched_files();
}

void FileModificationMonitor::process_event(const simlib_inotify_event& event) {
    const FileInfo* finfo = WONT_THROW(watched_files_.at(event.wd));
    if (event.mask & IN_IGNORED) {
        // File is no longer being watched
        watched_files_.erase(event.wd);
        unwatched_files_.emplace(finfo);
        schedule_processing_unwatched_files();
        return;
    }

    if (event.mask & IN_MOVE_SELF) {
        // File has disappeared -- stop watching it
        if (inotify_rm_watch(intfy_fd_, event.wd)) {
            THROW("inotify_rm_watch()", errmsg());
        }
        // After removing watch IN_IGNORED event will be generated,
        // so the file's watching state will become unwatched on that event
        return;
    }

    if (not(event.mask & events_requiring_handler)) {
        THROW("read unknown event for ", finfo->path, ": mask = ", event.mask);
    }

    // File was modified
    if (event.file_name.has_value()) {
        // File inside watched directory
        const char* missing_slash = &"/"[has_suffix(finfo->path, "/")];
        run_modification_handler(finfo, concat_tostr(finfo->path, missing_slash, *event.file_name));
    } else {
        // Watched file or directory
        run_modification_handler(finfo, finfo->path);
    }
}

void FileModificationMonitor::schedule_processing_unwatched_files() {
    if (processing_unwatched_files_is_scheduled_ or unwatched_files_.empty()) {
        return;
    }

    eq_.add_repeating_handler(add_missing_files_retry_period_, [&] {
        process_unwatched_files(true);
        if (not unwatched_files_.empty()) {
            return continue_repeating;
        }

        processing_unwatched_files_is_scheduled_ = false;
        return stop_repeating;
    });
    processing_unwatched_files_is_scheduled_ = true;
}

void FileModificationMonitor::process_unwatched_files(bool run_modification_handler_on_success) {
    filter(unwatched_files_, [&](const FileInfo* finfo) {
        int wd = inotify_add_watch(intfy_fd_, finfo->path.data(), all_events);
        if (wd == -1) {
            watching_log_->could_not_watch(finfo->path, errno);
            return true;
        }

        // File is now watched
        watching_log_->started_watching(finfo->path);
        watched_files_.emplace(wd, finfo);
        if (run_modification_handler_on_success) {
            run_modification_handler(finfo, finfo->path);
        }

        return false;
    });
}

template <class String>
void FileModificationMonitor::run_modification_handler(const FileInfo* finfo, String&& file_path) {
    if (finfo->stillness_threshold == nanoseconds(0)) {
        event_handler_(file_path);
        return;
    }

    // Defer handling but remove previous deferred handling if it exists
    std::string file_path_to_use;
    if (auto node = deferred_modification_handlers_.extract(file_path); node.empty()) {
        file_path_to_use = std::forward<String>(file_path);
    } else {
        file_path_to_use = std::move(node.key());
        eq_.remove_handler(node.mapped());
    }

    // Schedule handling
    auto [it, inserted] =
        deferred_modification_handlers_.try_emplace(std::move(file_path_to_use), 0);
    assert(inserted);
    it->second = eq_.add_time_handler(finfo->stillness_threshold, [&, it = it] {
        event_handler_(it->first);
        deferred_modification_handlers_.erase(it);
    });
}
