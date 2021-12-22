#pragma once

#include "simlib/debug.hh"
#include "simlib/file_descriptor.hh"
#include "simlib/repeating.hh"

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <poll.h>
#include <set>
#include <type_traits>
#include <variant>
#include <vector>

enum class FileEvent {
    READABLE = 1 << 0,
    WRITEABLE = 1 << 1,
    CLOSED = 1 << 2, // Always can be returned
};

DECLARE_ENUM_UNARY_OPERATOR(FileEvent, ~)
DECLARE_ENUM_OPERATOR(FileEvent, |)
DECLARE_ENUM_OPERATOR(FileEvent, &)

class EventQueue {
public:
    using time_point = std::chrono::system_clock::time_point;
    using nanoseconds = std::chrono::nanoseconds;
    using handler_id_t = size_t;

private:
    struct TimedHandler {
        time_point time;
        std::function<void()> handler;
    };

    struct FileHandler {
        std::shared_ptr<std::function<void(FileEvent)>> handler;
        FileEvent events;
        size_t poll_event_idx;
    };

    handler_id_t next_handler_id_ = 0;

    std::map<handler_id_t, std::variant<TimedHandler, FileHandler>> handlers_;
    std::multiset<std::pair<time_point, handler_id_t>> timed_handlers_;

    std::vector<pollfd> poll_events_;
    std::vector<handler_id_t> poll_events_idx_to_hid_;

    std::atomic_bool immediate_pause_{};
    FileDescriptor immediate_pause_fd_;

    handler_id_t new_handler_id() noexcept { return next_handler_id_++; }

    [[nodiscard]] bool are_there_file_file_handlers() const noexcept {
        return (handlers_.size() > timed_handlers_.size());
    }

public:
    EventQueue();
    EventQueue(const EventQueue&) = delete;
    EventQueue(EventQueue&& other) noexcept;
    EventQueue& operator=(const EventQueue&) = delete;
    EventQueue& operator=(EventQueue&& other) noexcept;
    ~EventQueue() = default;

    // Stops processing of events immediately. It is safe to call it from other
    // threads.
    void pause_immediately() noexcept;

private:
    [[nodiscard]] bool immediate_pause_was_requested() const noexcept {
        return immediate_pause_.load(std::memory_order_acquire);
    }

public:
    handler_id_t add_ready_handler(std::function<void()> handler) {
        return add_time_handler_impl(std::chrono::system_clock::now(), std::move(handler));
    }

    handler_id_t add_time_handler(time_point tp, std::function<void()> handler);

    handler_id_t add_time_handler(
            time_point::duration duration, std::function<void()> handler);

    // Will repeat @p handler with pauses of @p interval until it @p handler
    // returns repeating::STOP.
    void add_repeating_handler(
            time_point::duration interval, std::function<repeating()> handler);

private:
    handler_id_t add_time_handler_impl(time_point tp, std::function<void()> handler);

    static decltype(pollfd::events) file_events_to_poll_events(FileEvent events) noexcept {
        decltype(pollfd::events) res = 0;
        if (uint(events & FileEvent::READABLE)) {
            res |= POLLIN;
        }
        if (uint(events & FileEvent::WRITEABLE)) {
            res |= POLLOUT;
        }
        return res;
    }

public:
    template <class Handler>
    handler_id_t add_file_handler(int fd, FileEvent events, Handler&& handler) {
        static_assert(std::is_invocable_r_v<void, Handler, FileEvent> or
                std::is_invocable_r_v<void, Handler>);

        STACK_UNWINDING_MARK;
        if constexpr (not std::is_invocable_r_v<void, Handler, FileEvent>) {
            STACK_UNWINDING_MARK;
            return add_file_handler(fd, events,
                    [handler = std::forward<Handler>(handler)](FileEvent /*unused*/) mutable {
                handler();
            });

        } else {
            auto handler_id = new_handler_id();
            handlers_.emplace(handler_id,
                    FileHandler{std::make_shared<std::function<void(FileEvent)>>(
                                        std::forward<Handler>(handler)),
                            events, poll_events_.size()});
            try {
                poll_events_idx_to_hid_.emplace_back(handler_id);
                try {
                    poll_events_.push_back({fd, file_events_to_poll_events(events), 0});
                    return handler_id;
                } catch (...) {
                    poll_events_idx_to_hid_.pop_back();
                    throw;
                }
            } catch (...) {
                handlers_.erase(handler_id);
                throw;
            }
        }
    }

    // Removing handler within itself is undefined behavior except for file
    // handlers
    void remove_handler(handler_id_t handler_id);

    // After an immediate pause it may be called again to continue running as if
    // the immediate pause did not happen
    void run();
};
