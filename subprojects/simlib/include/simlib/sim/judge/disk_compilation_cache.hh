#pragma once

#include <chrono>
#include <exception>
#include <simlib/sim/judge/compilation_cache.hh>
#include <string>

namespace sim::judge {

class DiskCompilationCache : public CompilationCache {
    std::string cache_dir;
    std::chrono::seconds max_staleness;

public:
    explicit DiskCompilationCache(std::string cache_dir, std::chrono::seconds max_staleness);

    bool copy_from_cache_if_newer_than(
        std::string_view name, FilePath dest, const std::chrono::system_clock::time_point& tp
    ) override;

    void save_or_override(std::string_view name, FilePath src) override;
};

} // namespace sim::judge
