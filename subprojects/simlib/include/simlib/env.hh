#pragma once

#include <cstdlib>
#include <optional>

// This function is thread-safe only if the other threads don't modify the environment
inline std::optional<const char*> get_env_var(const char* name) noexcept {
    auto* p = std::getenv(name); // NOLINT(concurrency-mt-unsafe)
    if (!p) {
        return std::nullopt;
    }
    return p;
}
