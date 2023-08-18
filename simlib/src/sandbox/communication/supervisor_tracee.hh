#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <optional>
#include <simlib/array_vec.hh>
#include <simlib/concat_common.hh>
#include <simlib/meta/min.hh>
#include <simlib/uninitialized_aligned_storage.hh>
#include <type_traits>

namespace sandbox::communication::supervisor_tracee {

static constexpr auto shared_mem_state_sizeof = 4096;

struct SharedMemState {
    uint16_t error_len; // 0 indicates no error
    UninitializedAlignedStorage<std::byte, shared_mem_state_sizeof - sizeof(error_len)>
        error_description;
};

static_assert(sizeof(SharedMemState) == shared_mem_state_sizeof);

inline volatile SharedMemState* initialize(void* shared_mem_state_raw) noexcept {
    if (reinterpret_cast<std::uintptr_t>(shared_mem_state_raw) % alignof(SharedMemState) != 0) {
        std::terminate();
    }

    static_assert(
        std::is_trivially_destructible_v<SharedMemState>, "This value won't be destructed"
    );
    return new (shared_mem_state_raw) SharedMemState{
        .error_len = 0,
        .error_description = {},
    };
}

inline void write_no_error(volatile SharedMemState* shared_mem_state) noexcept {
    shared_mem_state->error_len = 0;
}

template <class... Args>
void write_error(volatile SharedMemState* shared_mem_state, Args&&... msg) noexcept {
    size_t bytes_written = 0;
    auto append = [&](auto&& str) noexcept {
        size_t len = meta::min(
            string_length(str), sizeof(SharedMemState::error_description) - bytes_written
        );
        for (size_t i = 0; i < len; ++i) {
            shared_mem_state->error_description.data()[bytes_written] =
                static_cast<std::byte>(data(str)[i]);
            ++bytes_written;
        }
    };
    (append(stringify(std::forward<Args>(msg))), ...);
    shared_mem_state->error_len = bytes_written;
}

inline std::optional<ArrayVec<char, sizeof(SharedMemState::error_description)>>
read_error(volatile const SharedMemState* shared_mem_state) noexcept {
    auto error_len = shared_mem_state->error_len;
    if (error_len == 0) {
        return std::nullopt;
    }
    if (error_len > sizeof(SharedMemState::error_description)) {
        std::terminate(); // BUG
    }
    ArrayVec<char, sizeof(SharedMemState::error_description)> res;
    for (size_t i = 0; i < error_len; ++i) {
        res.push(static_cast<char>(shared_mem_state->error_description.data()[i]));
    }
    return res;
}

inline void reset(volatile SharedMemState* shared_mem_state) noexcept {
    // First, erase all the remnants of the previous state (for security)
    std::memset(const_cast<SharedMemState*>(shared_mem_state), 0, sizeof(*shared_mem_state));
    // Reinitialize state
    write_no_error(shared_mem_state);
}

} // namespace sandbox::communication::supervisor_tracee
