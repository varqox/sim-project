#pragma once

#include "semaphore.hh"

#include <climits>
#include <deque>
#include <mutex>
#include <type_traits>

namespace concurrent {

template <class Elem>
class BoundedQueue {
private:
	Semaphore elems_limit_;
	Semaphore queued_elems_ {0};
	std::mutex lock_;
	std::atomic_bool no_more_elems_ = false;
	std::deque<Elem> elems_;

	Elem pop_elem() {
		auto elem = [&] {
			if constexpr (std::is_move_constructible_v<Elem>)
				return std::move(elems_.front());
			else
				return elems_.front();
		}();

		elems_.pop_front();
		elems_limit_.post();
		return elem;
	}

	Elem extract_elem() {
		std::lock_guard<std::mutex> guard(lock_);
		if (not elems_.empty())
			return pop_elem();

		queued_elems_.post();
		throw NoMoreElems();
	}

	std::optional<Elem> extract_elem_opt() {
		std::lock_guard<std::mutex> guard(lock_);
		if (not elems_.empty())
			return pop_elem();

		queued_elems_.post();
		return std::nullopt;
	}

public:
	struct NoMoreElems {};

	explicit BoundedQueue(unsigned size = SEM_VALUE_MAX) : elems_limit_(size) {}

	BoundedQueue(const BoundedQueue&) = delete;
	BoundedQueue(BoundedQueue&&) = delete;
	BoundedQueue& operator=(const BoundedQueue&) = delete;
	BoundedQueue& operator=(BoundedQueue&&) = delete;

	bool signaled_no_more_elems() const noexcept { return no_more_elems_; }

	// Throws NoMoreElements iff there are no more elements and
	// signal_no_more_elements() was called
	Elem pop() {
		queued_elems_.wait();
		return extract_elem();
	}

	// Returns std::nullopt iff there are no more elements and
	// signal_no_more_elements() was called
	std::optional<Elem> pop_opt() {
		queued_elems_.wait();
		return extract_elem_opt();
	}

	// Gets the element or returns std::nullopt immediately. Throws
	// NoMoreElements if there are no more jobs and signal_no_more_elems() has
	// been called.
	std::optional<Elem> try_pop() {
		if (queued_elems_.try_wait())
			return extract_elem();

		return std::nullopt;
	}

	// Gets the element or returns std::nullopt immediately. Returns
	// std::nullopt if there are no more jobs and signal_no_more_elems() has
	// been called. To determine which case happened on std::nullopt,
	// signaled_no_more_elems() should be used.
	std::optional<Elem> try_pop_opt() {
		if (queued_elems_.try_wait())
			return extract_elem_opt();

		return std::nullopt;
	}

	void push(Elem elem) {
		elems_limit_.wait();
		std::lock_guard<std::mutex> guard(lock_);
		elems_.emplace_back(std::move(elem));
		queued_elems_.post();
	}

	// Calling push() after this method is forbidden. Can be called more
	// than once
	void signal_no_more_elems() {
		no_more_elems_ = true;
		queued_elems_.post();
	}

	void increment_queue_size(size_t times = 1) {
		while (times) {
			elems_limit_.post();
			--times;
		}
	}
};

} // namespace concurrent
