#pragma once

#include "avl_dict.hh"
#include "concat.hh"
#include "debug.hh"
#include "file_descriptor.hh"
#include "inplace_buff.hh"
#include "logger.hh"
#include "memory.hh"
#include "string_traits.hh"
#include "string_view.hh"

#include <bits/stdint-uintn.h>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <linux/limits.h>
#include <string_view>
#include <sys/inotify.h>
#include <sys/poll.h>
#include <type_traits>
#include <vector>

class SimpleWatchingLog {
public:
	template <class... Args>
	void warning(Args&&... args) {
		stdlog("warning: ", std::forward<Args>(args)...);
	}

	void started_watching(CStringView file) {
		stdlog("Started watching: ", file);
	}
};

class SilentWatchingLog {
public:
	template <class... Args>
	void warning(Args&&...) {}

	void started_watching(CStringView) {}
};

/**
 * @brief Watches files and directories specified as @p files and calls
 *   @p event_handler for every creation / modification (excluding rename
 *   and deletion) event on watched files or files directly inside
 *   watched directories
 *
 * @param files path of files and directories to watch (it is not recursive)
 * @param watching_log logger for warnings and start-watching events
 * @param event_handler callback to run on every modification / creation
 *   (excluding rename or deletion) event on watched files. It should accept
 *   CStringView as an argument that denotes a path of file corresponding to
 *   this event. This path will end with '/' iff the corresponding file is
 *   a directory.
 *
 * @errors will be thrown as std::runtime_exception with appropriate message.
 */
template <class Handler, class WatchingLog>
void watch_files_for_modification(const std::vector<std::string>& files,
                                  WatchingLog&& watching_log,
                                  Handler&& event_handler) {
	STACK_UNWINDING_MARK;
	static_assert(std::is_invocable_v<Handler, CStringView>);
	FileDescriptor ino_fd(inotify_init());
	if (ino_fd == -1)
		THROW("inotify_init()", errmsg());

	AVLDictSet<CStringView> unwatched_files;
	for (CStringView file : files)
		unwatched_files.emplace(file);

	constexpr auto events_requiring_handler = IN_MODIFY | // modification
	                                          IN_CREATE |
	                                          IN_MOVED_TO; // file creation

	constexpr auto all_events = events_requiring_handler |
	                            IN_MOVE_SELF | // deletion
	                            IN_EXCL_UNLINK; // do not monitor unlinked files

	AVLDictMap<int, CStringView> watched_files; // wd => file path
	bool starting_watching_is_a_modification = false;
	auto process_unwatched_files = [&] {
		unwatched_files.filter([&](CStringView file) {
			int wd = inotify_add_watch(ino_fd, file.data(), all_events);
			if (wd == -1) {
				watching_log.warning("could not watch file ", file,
				                     ": inotify_add_watch()", errmsg());
				return false;
			}

			// File is now watched
			watching_log.started_watching(file);
			watched_files.emplace(wd, file);
			if (starting_watching_is_a_modification)
				event_handler(file);

			return true;
		});
	};

	process_unwatched_files();
	starting_watching_is_a_modification = false;

	using std::chrono::milliseconds;
	constexpr auto DONT_TRY_TO_WATCH_UNWATCHED_FILES_TIMEOUT = milliseconds(50);
	constexpr size_t inotify_buff_len =
	   (sizeof(inotify_event) + PATH_MAX + 1) * 16;
	std::unique_ptr<char[], delete_using_free> inotify_buff(
	   (char*)std::aligned_alloc(sizeof(inotify_event), inotify_buff_len));
	for (;;) {
		// Wait for notification
		pollfd pfd = {ino_fd, POLLIN, 0};
		int rc =
		   poll(&pfd, 1,
		        (unwatched_files.empty()
		            ? -1
		            : milliseconds(DONT_TRY_TO_WATCH_UNWATCHED_FILES_TIMEOUT)
		                 .count()));
		if (rc < 0)
			THROW("poll()", errmsg());

		if (rc == 0) { // Timeouted
			process_unwatched_files();
			continue;
		}

		assert(rc == 1);
		ssize_t read_len = read(ino_fd, inotify_buff.get(), inotify_buff_len);
		if (read_len < 1) {
			watching_log.warning("read()", errmsg());
			continue;
		}

		char* ptr = inotify_buff.get();
		while (ptr < inotify_buff.get() + read_len) {
			decltype(inotify_event::wd) wd;
			decltype(inotify_event::mask) mask;
			// decltype(inotify_event::cookie) cookie; // ignored
			decltype(inotify_event::len) len;
			std::memcpy(&wd, ptr + offsetof(inotify_event, wd), sizeof(wd));
			std::memcpy(&mask, ptr + offsetof(inotify_event, mask),
			            sizeof(mask));
			std::memcpy(&len, ptr + offsetof(inotify_event, len), sizeof(len));
			StringView name(ptr + offsetof(inotify_event, name));
			static_assert(offsetof(inotify_event, name) ==
			              sizeof(inotify_event));
			ptr += sizeof(inotify_event) + len;

			// Process event
			bool file_inside_dir = (len > 0);
			CStringView wd_name = watched_files.find(wd)->second;

			stdlog(wd, ": ", wd_name, ": mask = ", mask);

			if (mask & IN_IGNORED) {
				// File is no longer being watched
				watched_files.erase(wd);
				unwatched_files.emplace(wd_name);
			} else if (mask & IN_MOVE_SELF) {
				// File has disappeared -- stop watching it
				if (inotify_rm_watch(ino_fd, wd))
					THROW("inotify_rm_watch()", errmsg());
				// After removing watch an IN_IGNORED event will be generated,
				// so the file will be unwatched in cache on that event
			} else if (mask & events_requiring_handler) {
				if (not file_inside_dir) {
					if ((~mask & IN_ISDIR) or has_suffix(wd_name, "/")) {
						event_handler(wd_name);
					} else {
						auto path = concat(wd_name, '/');
						event_handler(path.to_cstr());
					}
				} else {
					auto path =
					   concat(wd_name, (has_suffix(wd_name, "/") ? "" : "/"),
					          name, (mask & IN_ISDIR ? "/" : ""));
					event_handler(path.to_cstr());
				}
			} else {
				THROW("read unknown event for ", wd_name, ": mask = ", mask);
			}
		}
	}
}
