#pragma once

#include "simlib/debug.hh"
#include "simlib/file_descriptor.hh"
#include "simlib/meta.hh"
#include "simlib/overloaded.hh"
#include "simlib/repeating.hh"
#include "simlib/shared_function.hh"
#include "simlib/time.hh"

#include <atomic>
#include <chrono>
#include <fcntl.h>
#include <functional>
#include <map>
#include <memory>
#include <poll.h>
#include <ratio>
#include <set>
#include <sys/eventfd.h>
#include <sys/poll.h>
#include <thread>
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

	std::atomic_bool immediate_pause_;
	FileDescriptor immediate_pause_fd_;

	handler_id_t new_handler_id() noexcept { return next_handler_id_++; }

	bool are_there_file_file_handlers() const noexcept {
		return (handlers_.size() > timed_handlers_.size());
	}

public:
	EventQueue() : immediate_pause_fd_(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)) {
		if (not immediate_pause_fd_.is_open())
			THROW("eventfd()", errmsg());

		// Add handler to make ppoll() wake if an immediate pause is requested
		add_file_handler(immediate_pause_fd_, FileEvent::READABLE, [this] {
			// If this happens before the memory is synchronized with thread
			// requesting pause  thread, be sure to set this flag, so that it is
			// indeed an *immediate* pause
			immediate_pause_.store(true, std::memory_order_release);
		});
	}

	EventQueue(const EventQueue&) = delete;

	EventQueue(EventQueue&& other) noexcept
	   : next_handler_id_(std::move(other.next_handler_id_)),
	     handlers_(std::move(other.handlers_)),
	     timed_handlers_(std::move(other.timed_handlers_)),
	     poll_events_(std::move(other.poll_events_)),
	     poll_events_idx_to_hid_(std::move(other.poll_events_idx_to_hid_)),
	     immediate_pause_(
	        other.immediate_pause_.load(std::memory_order_acquire)),
	     immediate_pause_fd_(std::move(other.immediate_pause_fd_)) {}

	EventQueue& operator=(const EventQueue&) = delete;

	EventQueue& operator=(EventQueue&& other) {
		next_handler_id_ = std::move(other.next_handler_id_);
		handlers_ = std::move(other.handlers_);
		timed_handlers_ = std::move(other.timed_handlers_);
		poll_events_ = std::move(other.poll_events_);
		poll_events_idx_to_hid_ = std::move(other.poll_events_idx_to_hid_);
		immediate_pause_.store(
		   other.immediate_pause_.load(std::memory_order_acquire),
		   std::memory_order_release);
		immediate_pause_fd_ = std::move(other.immediate_pause_fd_);
		return *this;
	}

	// Stops processing of events immediately. It is safe to call it from other
	// threads.
	void pause_immediately() noexcept {
		(void)immediate_pause_.exchange(true, std::memory_order_acq_rel);
		int fd = immediate_pause_fd_;
		assert(fd >= 0); // Make sure memory ordering "acquire" worked
		uint64_t x = 1;
		(void)write(fd, &x, sizeof(x));
	}

private:
	bool immediate_pause_was_requested() const noexcept {
		return immediate_pause_.load(std::memory_order_acquire);
	}

public:
	handler_id_t add_ready_handler(std::function<void()> handler) {
		return add_time_handler_impl(std::chrono::system_clock::now(),
		                             std::move(handler));
	}

	handler_id_t add_time_handler(time_point tp,
	                              std::function<void()> handler) {
		STACK_UNWINDING_MARK;
		const auto now = std::chrono::system_clock::now();
		if (tp < now)
			tp = now; // Disallow potential starvation of other handlers

		return add_time_handler_impl(tp, std::move(handler));
	}

	handler_id_t add_time_handler(time_point::duration duration,
	                              std::function<void()> handler) {
		STACK_UNWINDING_MARK;
		assert(duration >= decltype(duration)::zero());
		return add_time_handler_impl(
		   std::chrono::system_clock::now() + duration, std::move(handler));
	}

	// Will repeat @p handler with pauses of @p interval until it @p handler
	// returns repeating::STOP.
	void add_repeating_handler(time_point::duration interval,
	                           std::function<repeating()> handler) {
		STACK_UNWINDING_MARK;
		assert(interval >= decltype(interval)::zero());
		auto impl = [this, interval,
		             handler =
		                shared_function(std::move(handler))](auto& self) {
			if (handler() == stop_repeating)
				return;

			// Continue repeating
			add_time_handler(interval, [self] { self(self); });
		};

		add_time_handler(interval, [impl = std::move(impl)] { impl(impl); });
	}

private:
	handler_id_t add_time_handler_impl(time_point tp,
	                                   std::function<void()> handler) {
		const auto handler_id = new_handler_id();
		handlers_.emplace(handler_id, TimedHandler {tp, std::move(handler)});
		try {
			timed_handlers_.emplace(tp, handler_id);
			return handler_id;
		} catch (...) {
			handlers_.erase(handler_id);
			throw;
		}
	}

	static decltype(pollfd::events)
	file_events_to_poll_events(FileEvent events) noexcept {
		decltype(pollfd::events) res = 0;
		if (uint(events & FileEvent::READABLE))
			res |= POLLIN;
		if (uint(events & FileEvent::WRITEABLE))
			res |= POLLOUT;
		return res;
	}

	static FileEvent
	poll_events_to_file_events(decltype(pollfd::revents) events) noexcept {
		FileEvent res = FileEvent(0);
		if (events & POLLIN)
			res = res | FileEvent::READABLE;
		if (events & POLLOUT)
			res = res | FileEvent::WRITEABLE;
		if (events & (POLLERR | POLLHUP))
			res = res | FileEvent::CLOSED;
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
			                        [handler = std::forward<Handler>(handler)](
			                           FileEvent) mutable { handler(); });

		} else {
			auto handler_id = new_handler_id();
			handlers_.emplace(
			   handler_id,
			   FileHandler {std::make_shared<std::function<void(FileEvent)>>(
			                   std::move(handler)),
			                events, poll_events_.size()});
			try {
				poll_events_idx_to_hid_.emplace_back(handler_id);
				try {
					poll_events_.push_back(
					   {fd, file_events_to_poll_events(events), 0});
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
	void remove_handler(handler_id_t handler_id) {
		STACK_UNWINDING_MARK;

		std::visit(
		   overloaded {[&](TimedHandler& handler) {
			               timed_handlers_.erase({handler.time, handler_id});
		               },
		               [&](FileHandler& handler) {
			               poll_events_[handler.poll_event_idx].fd =
			                  -1; // Deactivate event. It will be removed while
			                      // processing file events.
		               }},
		   WONT_THROW(handlers_.at(handler_id)));
		handlers_.erase(handler_id);
	}

	// After an immediate pause it may be called again to continue running as if
	// the immediate pause did not happen
	void run() {
		STACK_UNWINDING_MARK;

		// Reset pauseping
		immediate_pause_.store(false, std::memory_order_release);
		{
			// Reset immediate_pause_fd_
			uint64_t x;
			(void)read(immediate_pause_fd_, &x, sizeof(x)); // It is nonblocking
		}

		constexpr auto TIME_QUANTUM = std::chrono::microseconds(6);
		auto process_timed_handlers = [&] {
			STACK_UNWINDING_MARK;

			if (timed_handlers_.empty() or immediate_pause_was_requested())
				return;

			const auto start = std::chrono::system_clock::now();
			auto now = start;
			do {
				if (timed_handlers_.empty())
					break;

				const auto [tp, handler_id] = *timed_handlers_.begin();
				if (tp > now)
					break;

				timed_handlers_.erase(timed_handlers_.begin());
				const auto it = handlers_.find(handler_id);
				assert(it != handlers_.end());
				auto handler =
				   std::move(std::get<TimedHandler>(it->second).handler);
				handlers_.erase(it);

				handler(); // It is ok if it throws
				if (immediate_pause_was_requested())
					return;
			} while ((now = std::chrono::system_clock::now()) <=
			         start + TIME_QUANTUM);
		};

		auto process_file_events = [&] {
			STACK_UNWINDING_MARK;
			if (immediate_pause_was_requested())
				return;

			auto file_handlers_quantum_start = std::chrono::system_clock::now();
			size_t new_poll_events_size =
			   0; // Expired events are removed and the array is compressed

			// poll_events_ may get new elements as we process event handler,
			// even get reallocated, we need to be careful about pointers and
			// references to poll_events_ elements. Also we compress the whole
			// array, so we need to process even the new elements that were
			// nonexistent at the start
			for (size_t idx = 0; idx < poll_events_.size(); ++idx) {
				if (poll_events_[idx].fd < 0)
					continue;

				const size_t new_idx = new_poll_events_size++;
				if (new_idx != idx) {
					// Move entry
					poll_events_[new_idx] = poll_events_[idx];
					const auto handler_id =
					   WONT_THROW(poll_events_idx_to_hid_.at(idx));
					poll_events_idx_to_hid_[new_idx] = handler_id;
					WONT_THROW(std::get<FileHandler>(handlers_.at(handler_id))
					              .poll_event_idx) = new_idx;
					// Disable old entry (in case something below throws)
					poll_events_[idx].fd = -1;
				}

				// No event
				const auto revents = poll_events_[new_idx].revents;
				if (revents == 0)
					continue;

				if (std::chrono::system_clock::now() >
				    file_handlers_quantum_start + TIME_QUANTUM) {
					// Needed to ensure low latency
					process_timed_handlers();
					if (immediate_pause_was_requested())
						return;

					file_handlers_quantum_start =
					   std::chrono::system_clock::now();
					if (poll_events_[new_idx].fd < 0) {
						// Current event handler was deleted
						--new_poll_events_size;
						continue;
					}
				}

				// Process event
				FileEvent events = poll_events_to_file_events(revents);
				if (events != FileEvent(0)) {
					auto handler_id =
					   WONT_THROW(poll_events_idx_to_hid_.at(new_idx));

					auto handler_shr_ptr = WONT_THROW(
					   std::get<FileHandler>(handlers_.at(handler_id)).handler);
					(*handler_shr_ptr)(events);
					if (immediate_pause_was_requested())
						return;

					continue;
				}

				// Unhandled event
				if (revents & (POLLPRI | POLLRDHUP))
					continue; // Ignore these events

				THROW("pollfd.revents = ", revents,
				      " is an invalid event (at the moment of implementing "
				      "this) and cannot be handled");
			}

			poll_events_.resize(new_poll_events_size);
			poll_events_idx_to_hid_.resize(new_poll_events_size);
		};

		// There is always a handler for immediate_pause_fd_
		while (handlers_.size() > 1 and not immediate_pause_was_requested()) {
			const auto timeout = [&]() -> std::chrono::system_clock::duration {
				if (timed_handlers_.empty())
					return nanoseconds(-1);

				time_point first_expiration = timed_handlers_.begin()->first;
				time_point now = std::chrono::system_clock::now();
				if (first_expiration < now)
					return nanoseconds(0);

				return first_expiration - now;
			}();

			const int ready_poll_events_num = [&] {
				if (not are_there_file_file_handlers()) {
					if (timeout > nanoseconds(0))
						std::this_thread::sleep_for(timeout);

					return 0;
				}

				timespec ts;
				timespec* tmo_p;
				if (timeout < nanoseconds(0)) {
					tmo_p = nullptr;
				} else {
					tmo_p = &ts;
					ts = to_timespec(timeout);
				}

				return ppoll(poll_events_.data(), poll_events_.size(), tmo_p,
				             nullptr);
			}();
			if (ready_poll_events_num == -1 and errno != EINTR)
				THROW("ppoll()", errmsg());

			process_timed_handlers(); // It is important to first process timed
			                          // events, to not starve the timed events

			if (ready_poll_events_num > 0)
				process_file_events();
		}
	}
};
