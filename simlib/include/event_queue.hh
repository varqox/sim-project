#pragma once

#include "debug.hh"
#include "meta.hh"
#include "overloaded.hh"

#include <chrono>
#include <functional>
#include <map>
#include <poll.h>
#include <ratio>
#include <set>
#include <sys/poll.h>
#include <thread>
#include <variant>

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
	using milliseconds = std::chrono::milliseconds;
	using handler_id_t = size_t;

private:
	struct TimedHandler {
		time_point time;
		std::function<void()> handler;
	};

	struct FileHandler {
		std::function<void(FileEvent)> handler;
		FileEvent events;
		size_t poll_event_idx;
	};

	handler_id_t next_handler_id_ = 0;

	std::map<handler_id_t, std::variant<TimedHandler, FileHandler>> handlers_;
	std::multiset<std::pair<time_point, handler_id_t>> timed_handlers_;

	std::vector<pollfd> poll_events_;
	std::vector<handler_id_t> poll_events_idx_to_hid_;

	handler_id_t new_handler_id() noexcept { return next_handler_id_++; }

	bool are_there_file_file_handlers() const noexcept {
		return (handlers_.size() > timed_handlers_.size());
	}

public:
	handler_id_t add_ready_handler(std::function<void()> handler) {
		return add_time_handler({}, std::move(handler));
	}

	handler_id_t add_time_handler(time_point tp,
	                              std::function<void()> handler) {
		STACK_UNWINDING_MARK;
		const auto now = std::chrono::system_clock::now();
		if (tp < now)
			tp = now;

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

private:
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
	template <class Handler,
	          std::enable_if_t<std::is_invocable_r_v<void, Handler, FileEvent>,
	                           int> = 0>
	handler_id_t add_file_handler(int fd, FileEvent events, Handler&& handler) {
		STACK_UNWINDING_MARK;
		auto handler_id = new_handler_id();
		handlers_.emplace(handler_id, FileHandler {std::move(handler), events,
		                                           poll_events_.size()});
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

	template <class Handler,
	          std::enable_if_t<std::is_invocable_r_v<void, Handler>, int> = 0>
	handler_id_t add_file_handler(int fd, FileEvent events, Handler&& handler) {
		STACK_UNWINDING_MARK;
		return add_file_handler(
		   fd, events, [handler = std::forward<Handler>(handler)](FileEvent) {
			   handler();
		   });
	}

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

	void run() {
		STACK_UNWINDING_MARK;

		constexpr auto TIME_QUANTUM = std::chrono::microseconds(6);
		auto process_timed_handlers = [&] {
			STACK_UNWINDING_MARK;

			if (timed_handlers_.empty())
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
			} while ((now = std::chrono::system_clock::now()) <=
			         start + TIME_QUANTUM);
		};

		auto process_file_events = [&] {
			STACK_UNWINDING_MARK;
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
					// Disable old entry (in case something below throw)
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

					WONT_THROW(
					   std::get<FileHandler>(handlers_.at(handler_id)).handler)
					(events);
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

		while (not handlers_.empty()) {
			const auto timeout = [&]() -> std::chrono::system_clock::duration {
				if (timed_handlers_.empty())
					return milliseconds(-1);

				time_point first_expiration = timed_handlers_.begin()->first;
				time_point now = std::chrono::system_clock::now();
				if (first_expiration < now)
					return milliseconds(0);

				return first_expiration - now;
			}();

			const int ready_poll_events_num = [&] {
				if (not are_there_file_file_handlers()) {
					if (timeout > milliseconds(0))
						std::this_thread::sleep_for(timeout);

					return 0;
				}

				return poll(
				   poll_events_.data(), poll_events_.size(),
				   std::chrono::duration_cast<milliseconds>(timeout).count());
			}();
			if (ready_poll_events_num == -1 and errno != EINTR)
				THROW("poll()", errmsg());

			process_timed_handlers(); // It is important to first process timed
			                          // events, to not starve the timed events

			if (ready_poll_events_num > 0)
				process_file_events();
		}
	}
};
