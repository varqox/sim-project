#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <new>
#include <optional>
#include <simlib/always_false.hh>
#include <simlib/array_vec.hh>
#include <simlib/concat_common.hh>
#include <simlib/meta/min.hh>
#include <simlib/sandbox/si.hh>
#include <simlib/uninitialized_aligned_storage.hh>
#include <type_traits>
#include <variant>

namespace sandbox::communication::supervisor_pid1_tracee {

static constexpr auto shared_mem_state_sizeof = 4096;

struct SharedMemState {
    struct Time {
        int64_t seconds;
        uint32_t nanoseconds;
    };

    struct UsecTime {
        uint64_t usec;
    };

    Time tracee_exec_start_time;
    UsecTime tracee_exec_start_cpu_time_user;
    UsecTime tracee_exec_start_cpu_time_system;
    Time tracee_waitid_time;
    int16_t error_len; // < 0 indicates result::None

    union U {
        // error_len > 0
        UninitializedAlignedStorage<
            std::byte,
            shared_mem_state_sizeof - sizeof(Time) * 2 - sizeof(UsecTime) * 2 - alignof(Si)>
            error_description;
        // error_len == 0
        Si si;
    } u;
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
        .tracee_exec_start_time = {.seconds = -1, .nanoseconds = 0},
        .tracee_exec_start_cpu_time_user = {.usec = 0},
        .tracee_exec_start_cpu_time_system = {.usec = 0},
        .tracee_waitid_time = {.seconds = -1, .nanoseconds = 0},
        .error_len = -1,
        .u =
            {
                .error_description = {},
            },
    };
}

inline void write(volatile SharedMemState::Time& time, std::optional<timespec> ts) noexcept {
    time.seconds = ts ? ts->tv_sec : -1;
    time.nanoseconds = ts ? ts->tv_nsec : 0;
}

inline std::optional<timespec> read(volatile const SharedMemState::Time& time) noexcept {
    auto ts = timespec{
        .tv_sec = time.seconds,
        .tv_nsec = time.nanoseconds,
    };
    if (ts.tv_sec < 0) {
        return std::nullopt;
    }
    return ts;
}

inline void
write(volatile SharedMemState::UsecTime& usec_time, std::optional<uint64_t> usec) noexcept {
    usec_time.usec = usec ? *usec + 1 : 0;
}

inline std::optional<uint64_t> read(volatile const SharedMemState::UsecTime& usec_time) noexcept {
    auto usec = usec_time.usec;
    if (usec == 0) {
        return std::nullopt;
    }
    return usec - 1;
}

namespace result {
struct Ok {
    Si si;
};

struct Error {
    ArrayVec<char, sizeof(SharedMemState::U::error_description)> description;
};

struct None {};
} // namespace result

using Result = std::variant<result::Ok, result::Error, result::None>;

template <class ResKind>
inline bool is(volatile const SharedMemState* shared_mem_state) noexcept {
    if constexpr (std::is_same_v<ResKind, result::Ok>) {
        return shared_mem_state->error_len == 0;
    } else if constexpr (std::is_same_v<ResKind, result::Error>) {
        return shared_mem_state->error_len > 0;
    } else if constexpr (std::is_same_v<ResKind, result::None>) {
        return shared_mem_state->error_len < 0;
    } else {
        static_assert(always_false<ResKind>, "invalid ResKind");
    }
}

inline void write_result_ok(volatile SharedMemState* shared_mem_state, const Si& si) noexcept {
    shared_mem_state->u.si.code = si.code;
    shared_mem_state->u.si.status = si.status;
    shared_mem_state->error_len = 0;
}

template <class... Args>
void write_result_error(volatile SharedMemState* shared_mem_state, Args&&... msg) noexcept {
    size_t bytes_written = 0;
    auto append = [&](auto&& str) noexcept {
        size_t len = meta::min(
            string_length(str), sizeof(SharedMemState::U::error_description) - bytes_written
        );
        for (size_t i = 0; i < len; ++i) {
            shared_mem_state->u.error_description.data()[bytes_written] =
                static_cast<std::byte>(data(str)[i]);
            ++bytes_written;
        }
    };
    (append(stringify(std::forward<Args>(msg))), ...);
    shared_mem_state->error_len = static_cast<int16_t>(bytes_written);
}

inline void write_result_none(volatile SharedMemState* shared_mem_state) noexcept {
    shared_mem_state->error_len = -1;
}

inline Result read_result(volatile const SharedMemState* shared_mem_state) noexcept {
    auto error_len = shared_mem_state->error_len;
    if (error_len < 0) {
        return result::None{};
    }
    if (error_len == 0) {
        return result::Ok{
            .si =
                {
                    .code = shared_mem_state->u.si.code,
                    .status = shared_mem_state->u.si.status,
                },
        };
    }
    if (static_cast<size_t>(error_len) > sizeof(SharedMemState::U::error_description)) {
        std::terminate(); // BUG
    }
    result::Error err;
    for (size_t i = 0; i < static_cast<size_t>(error_len); ++i) {
        err.description.push(static_cast<char>(shared_mem_state->u.error_description.data()[i]));
    }
    return err;
}

inline void reset(volatile SharedMemState* shared_mem_state) noexcept {
    // First, erase all the remnants of the previous state (for security)
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
    std::memset(const_cast<SharedMemState*>(shared_mem_state), 0, sizeof(*shared_mem_state));
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    // Reinitialize state
    write(shared_mem_state->tracee_exec_start_time, std::nullopt);
    write(shared_mem_state->tracee_exec_start_cpu_time_user, std::nullopt);
    write(shared_mem_state->tracee_exec_start_cpu_time_system, std::nullopt);
    write(shared_mem_state->tracee_waitid_time, std::nullopt);
    write_result_none(shared_mem_state);
}

} // namespace sandbox::communication::supervisor_pid1_tracee
