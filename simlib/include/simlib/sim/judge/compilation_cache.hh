#pragma once

#include <chrono>
#include <simlib/file_path.hh>
#include <string_view>

namespace sim::judge {

class CompilationCache {
public:
    CompilationCache() = default;
    CompilationCache(const CompilationCache&) = delete;
    CompilationCache(CompilationCache&&) noexcept = default;
    CompilationCache& operator=(const CompilationCache&) = delete;
    CompilationCache& operator=(CompilationCache&&) = delete;
    virtual ~CompilationCache() = default;

    // Returns whether the copy actually took place i.e. the name exists in cache and is newer than
    // @p tp
    virtual bool copy_from_cache_if_newer_than(
        std::string_view name, FilePath dest, const std::chrono::system_clock::time_point& tp
    ) = 0;

    virtual void save_or_override(std::string_view name, FilePath src) = 0;
};

} // namespace sim::judge
