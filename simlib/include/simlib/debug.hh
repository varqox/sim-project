#pragma once

#include "simlib/concat.hh"
#include "simlib/concat_tostr.hh"
#include "simlib/inplace_array.hh"
#include "simlib/logger.hh"

#include <cstdlib>
#include <exception>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

#ifdef DEBUG
#define D(...) __VA_ARGS__
#define ND(...)
#else
#define D(...)
#define ND(...) __VA_ARGS__
#endif

#define STRINGIZE2(x) #x
#define STRINGIZE(x) STRINGIZE2(x)

#define LOG_LINE stdlog(__FILE__ ":" STRINGIZE(__LINE__))

#define E(...) eprintf(__VA_ARGS__)

#define throw_assert(expr)                                                     \
	((expr) ? (void)0                                                          \
	        : throw std::runtime_error(concat_tostr(                           \
	             __FILE__ ":" STRINGIZE(__LINE__) ": ", __PRETTY_FUNCTION__,   \
	             ": Assertion `" #expr "` failed.")))

// Very useful - includes exception origin
#define THROW(...)                                                             \
	throw std::runtime_error(concat_tostr(                                     \
	   __VA_ARGS__, " (thrown at " __FILE__ ":" STRINGIZE(__LINE__) ")"))

namespace simlib_debug {

constexpr bool is_VA_empty() { return true; }

template <class T1, class... T>
constexpr bool is_VA_empty(T1&&, T&&...) {
	return false;
}

constexpr const char* what_of() { return ""; }

inline const char* what_of(const std::exception& e) { return e.what(); }

} // namespace simlib_debug

inline InplaceBuff<4096> errmsg(int errnum) noexcept {
	static_assert(
	   to_string(std::numeric_limits<decltype(errnum)>::max()).size() + 1 <
	      90, // +1 for minus a sign
	   "Needed to fit error message in the returned buffer");
	std::array<char, 4000> buff;
	return concat<4096>(" - ", errnum, ": ",
	                    strerror_r(errnum, buff.data(), buff.size()));
}

inline auto errmsg() noexcept { return errmsg(errno); }

namespace stack_unwinding {

class StackGuard {
	static inline thread_local uintmax_t event_stamp;

	bool inside_catch_ = (bool)std::current_exception();
	int uncaught_counter_ = std::uncaught_exceptions();
	uintmax_t creation_stamp_ = event_stamp++;
	const char* file_;
	size_t line_;
	const char* pretty_func_;

public:
	struct StackMark {
		InplaceBuff<512> description;
		uintmax_t stamp = event_stamp++;

		template <class... Args,
		          std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
		StackMark(Args&&... args)
		   : description(std::in_place, std::forward<Args>(args)...) {}
	};

	static inline thread_local InplaceArray<StackMark, 128> marks_collected;

public:
	StackGuard(const char* file, size_t line,
	           const char* pretty_function) noexcept
	   : file_(file), line_(line), pretty_func_(pretty_function) {}

	StackGuard(const StackGuard&) = delete;
	StackGuard(StackGuard&&) = delete;
	StackGuard& operator=(const StackGuard&) = delete;
	StackGuard& operator=(StackGuard&&) = delete;

	~StackGuard() {
		if (std::uncaught_exceptions() == 1 and uncaught_counter_ == 0 and
		    not inside_catch_) {
			// Remove stack marks that are from earlier exceptions
			if (marks_collected.size() > 0 and
			    marks_collected.back().stamp < creation_stamp_) {
				marks_collected.clear();
			}

			try {
				StackMark a, b;
				assert(a.stamp + 1 == b.stamp);
				marks_collected.emplace_back(pretty_func_, " at ", file_, ':',
				                             line_);
			} catch (...) {
				// Nothing we can do here
			}
		}
	}
};

} // namespace stack_unwinding

#define STACK_UNWINDING_MARK_CONCATENATE_DETAIL(x, y) x##y
#define STACK_UNWINDING_MARK_CONCAT(x, y)                                      \
	STACK_UNWINDING_MARK_CONCATENATE_DETAIL(x, y)

#define STACK_UNWINDING_MARK                                                   \
	::stack_unwinding::StackGuard STACK_UNWINDING_MARK_CONCAT(                 \
	   stack_unwind_mark_no_, __COUNTER__)(__FILE__, __LINE__,                 \
	                                       __PRETTY_FUNCTION__)

#define ERRLOG_CATCH(...)                                                      \
	do {                                                                       \
		auto tmplog =                                                          \
		   errlog(__FILE__ ":" STRINGIZE(__LINE__) ": Caught exception",       \
		          ::simlib_debug::is_VA_empty(__VA_ARGS__) ? "" : " -> ",      \
		          ::simlib_debug::what_of(__VA_ARGS__),                        \
		          "\nStack unwinding marks:\n");                               \
                                                                               \
		size_t i = 0;                                                          \
		for (auto const& mark :                                                \
		     ::stack_unwinding::StackGuard::marks_collected)                   \
			tmplog('[', i++, "] ", mark.description, "\n");                    \
                                                                               \
		tmplog.flush_no_nl();                                                  \
		::stack_unwinding::StackGuard::marks_collected.clear();                \
	} while (false)

class ThrowingIsBug {
	bool thrown = true;

public:
	template <class T>
	decltype(auto) evaluate(T&& expr) noexcept {
		thrown = false;
		return std::forward<T>(expr);
	}

	~ThrowingIsBug() {
		if (thrown) {
			errlog("BUG: this was expected to not throw");
			std::abort();
		}
	}
};

// Sadly it won't preserve prvalue-ness, a prvalue will become an xvalue
#define WONT_THROW(...) (ThrowingIsBug().evaluate(__VA_ARGS__))
