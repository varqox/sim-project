#pragma once

#include <cstddef>
#include <exception>
#include <simlib/string_traits.hh>

namespace sim::mysql {

template <class Func> // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
void repeat_if_deadlocked(size_t deadlocks_limit, Func&& func) {
    for (size_t deadlocks = 0;; ++deadlocks) {
        try {
            func();
            return;
        } catch (const std::exception& e) {
            if (has_prefix(
                    e.what(), "Deadlock found when trying to get lock; try restarting transaction"
                ) &&
                deadlocks < deadlocks_limit)
            {
                continue;
            }
            throw;
        }
    }
}

} // namespace sim::mysql
