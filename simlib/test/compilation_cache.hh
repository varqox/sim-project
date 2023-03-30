#pragma once

#include <chrono>
#include <simlib/concat_tostr.hh>
#include <simlib/debug.hh>
#include <simlib/file_info.hh>
#include <simlib/file_manip.hh>
#include <simlib/file_path.hh>
#include <simlib/sim/judge_worker.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_view.hh>
#include <simlib/time.hh>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

class CompilationCache {
    const std::string cache_dir_;
    const std::optional<std::chrono::seconds> valid_duration_;

public:
    CompilationCache(std::string cache_dir, std::optional<std::chrono::seconds> valid_duration)
    : cache_dir_(std::move(cache_dir))
    , valid_duration_(valid_duration) {
        assert(has_suffix(cache_dir_, "/"));
    }

    [[nodiscard]] bool
    is_cached(StringView in_cache_path, std::chrono::system_clock::time_point file_mtime) const {
        struct stat64 cached_stat = {};
        if (stat64(FilePath(cached_path(in_cache_path)), &cached_stat)) {
            if (errno == ENOENT) {
                return false; // No record in cache
            }
            THROW("stat64()", errmsg());
        }

        auto cached_mtime = std::chrono::system_clock::time_point(to_duration(cached_stat.st_mtim));
        if (cached_mtime < file_mtime) {
            return false; // Cached file is older than the source file
        }
        if (valid_duration_ and std::chrono::system_clock::now() - cached_mtime > *valid_duration_)
        {
            return false; // Cached file has expired
        }
        return true;
    }

    [[nodiscard]] bool is_cached(StringView in_cache_path, FilePath file_path) const {
        return is_cached(in_cache_path, get_modification_time(file_path));
    }

    [[nodiscard]] std::string cached_path(StringView in_cache_path) const {
        return concat_tostr(cache_dir_, in_cache_path);
    }

    // NOLINTNEXTLINE(readability-make-member-function-const)
    void cache_file(StringView in_cache_path, FilePath file_path) {
        auto path = cached_path(in_cache_path);
        if (create_subdirectories(path) == -1) {
            THROW("create_subdirectories(", path, ')', errmsg());
        }
        if (copy_using_rename(file_path, path)) {
            THROW("copy_using_rename()", errmsg());
        }
    }

    // NOLINTNEXTLINE(readability-make-member-function-const)
    void cache_compiled_checker(StringView in_cache_path, sim::JudgeWorker& jworker) {
        auto path = cached_path(in_cache_path);
        if (create_subdirectories(path) == -1) {
            THROW("create_subdirectories(", path, ')', errmsg());
        }
        jworker.save_compiled_checker(path, copy_using_rename);
    }

    // NOLINTNEXTLINE(readability-make-member-function-const)
    void cache_compiled_solution(StringView in_cache_path, sim::JudgeWorker& jworker) {
        auto path = cached_path(in_cache_path);
        if (create_subdirectories(path) == -1) {
            THROW("create_subdirectories(", path, ')', errmsg());
        }
        jworker.save_compiled_solution(path, copy_using_rename);
    }
};
