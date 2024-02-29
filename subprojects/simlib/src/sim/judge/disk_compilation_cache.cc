#include <cerrno>
#include <cstdlib>
#include <simlib/errmsg.hh>
#include <simlib/file_info.hh>
#include <simlib/file_manip.hh>
#include <simlib/file_perms.hh>
#include <simlib/macros/throw.hh>
#include <simlib/sim/judge/disk_compilation_cache.hh>

static std::string to_cached_path(std::string_view name) {
    std::string res;
    for (auto c : name) {
        if (c == '\\') {
            res += "\\\\";
        } else if (c == ',') {
            res += "\\,";
        } else if (c == '/') {
            res += ',';
        } else {
            res += c;
        }
    }
    return res;
}

namespace sim::judge {

DiskCompilationCache::DiskCompilationCache(
    std::string cache_dir, std::chrono::seconds max_staleness
)
: cache_dir{std::move(cache_dir)}
, max_staleness{max_staleness} {
    if (this->cache_dir.empty()) {
        std::terminate();
    }
    if (this->cache_dir.back() != '/') {
        this->cache_dir += '/';
    }
    if (mkdir(this->cache_dir) && errno != EEXIST) {
        THROW("mkdir()", errmsg());
    }
}

bool DiskCompilationCache::copy_from_cache_if_newer_than(
    std::string_view name, FilePath dest, const std::chrono::system_clock::time_point& tp
) {
    auto path = cache_dir + to_cached_path(name);
    struct stat64 st = {};
    if (stat64(path.c_str(), &st) == -1) {
        if (errno == ENOENT) {
            return false;
        }
        THROW("stat64()", errmsg());
    }
    auto mtime = get_modification_time(st);
    if (mtime > tp && std::chrono::system_clock::now() - mtime <= max_staleness) {
        thread_fork_safe_copy(path, dest, S_0755);
        return true;
    }
    return false;
}

void DiskCompilationCache::save_or_override(std::string_view name, FilePath src) {
    auto path = cache_dir + to_cached_path(name);
    if (copy_using_rename(src, path)) {
        THROW("copy_using_rename()");
    }
}

} // namespace sim::judge
