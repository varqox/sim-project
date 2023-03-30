#pragma once

#include <climits>
#include <deque>
#include <mutex>
#include <simlib/concurrent/mutexed_value.hh>
#include <simlib/concurrent/semaphore.hh>
#include <type_traits>

namespace concurrent {

template <class Elem>
class BoundedQueue {
private:
    Semaphore free_slots_;
    Semaphore queued_elems_{0};
    MutexedValue<std::deque<Elem>> elems_;

    Elem pop_elem(typename decltype(elems_)::value_type& elems) {
        auto elem = [&] {
            if constexpr (std::is_move_constructible_v<Elem>) {
                return std::move(elems.front());
            } else {
                return elems.front();
            }
        }();

        elems.pop_front();
        free_slots_.post();
        return elem;
    }

    Elem extract_elem() {
        return elems_.perform([&](auto& elems) {
            if (not elems.empty()) {
                return pop_elem(elems);
            }

            queued_elems_.post();
            throw NoMoreElems();
        });
    }

    std::optional<Elem> extract_elem_opt() {
        return elems_.perform([&](auto& elems) -> std::optional<Elem> {
            if (not elems.empty()) {
                return pop_elem(elems);
            }

            queued_elems_.post();
            return std::nullopt;
        });
    }

public:
    struct NoMoreElems {};

    explicit BoundedQueue(unsigned max_size = SEM_VALUE_MAX) : free_slots_(max_size) {}

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue(BoundedQueue&&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;
    BoundedQueue& operator=(BoundedQueue&&) = delete;

    ~BoundedQueue() = default;

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

    // Gets the element or returns std::nullopt immediately. If NoMoreElements
    // is thrown, then there are no more jobs and signal_no_more_elems() has
    // been called. Notice that this does not mean that this function will not
    // return std::nullopt after signal_no_more_elements() is called -- this
    // case is entirely possible and you must prepare for it.
    std::optional<Elem> try_pop() {
        if (queued_elems_.try_wait()) {
            return extract_elem();
        }

        return std::nullopt;
    }

    void push(Elem elem) {
        free_slots_.wait();
        elems_.perform([&](auto& elems) {
            elems.emplace_back(std::move(elem));
            queued_elems_.post();
        });
    }

    // Calling push() after this method is forbidden. This method may be called
    // more than once
    void signal_no_more_elems() { queued_elems_.post(); }

    void increment_queue_size(size_t times = 1) {
        while (times) {
            free_slots_.post();
            --times;
        }
    }
};

} // namespace concurrent
