#pragma once
#include "logger.h"
#include "string.h"
#include "utilities.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
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

#define LOG_LINE stdlog(__FILE__ ":" STRINGIZE(__LINE__))

#define E(...) eprintf(__VA_ARGS__)

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

constexpr const char* __what() { return ""; }

inline const char* __what(const std::exception& e) { return e.what(); }

} // namespace simlib_debug

#define ERRLOG_CATCH(...)                                                      \
	do {                                                                       \
		auto tmplog = errlog(                                                  \
		   __FILE__ ":" STRINGIZE(__LINE__) ": Caught exception",              \
		   ::simlib_debug::is_VA_empty(__VA_ARGS__) ? "" : " -> ",             \
		   ::simlib_debug::__what(__VA_ARGS__), "\nStack unwinding marks:\n"); \
                                                                               \
		size_t i = 0;                                                          \
		for (auto&& mark : stack_unwinding::marks_collected)                   \
			tmplog('[', i++, "] ", mark, "\n");                                \
                                                                               \
		stack_unwinding::marks_collected.clear();                              \
	} while (false)

#define ERRLOG_FORWARDING(...)                                                 \
	errlog(__FILE__ ":" STRINGIZE(__LINE__) ": Forwarding exception...",       \
	       ::simlib_debug::is_VA_empty(__VA_ARGS__) ? "" : " -> ",             \
	       ::simlib_debug::__what(__VA_ARGS__))

#define ERRLOG_AND_FORWARD(...)                                                \
	{                                                                          \
		ERRLOG_FORWARDING(__VA_ARGS__);                                        \
		throw;                                                                 \
	}

inline InplaceBuff<4096> errmsg(int errnum) noexcept {
	static_assert(
	   string_length(
	      meta::ToString<std::numeric_limits<decltype(errnum)>::max()> {}) +
	         1 <
	      90, // +1 for minus a sign
	   "Needed to fit error message in the returned buffer");
	std::array<char, 4000> buff;
	return concat<4096>(" - ", errnum, ": ",
	                    strerror_r(errnum, buff.data(), buff.size()));
}

inline auto errmsg() noexcept { return errmsg(errno); }

#if !defined(__cpp_lib_uncaught_exceptions) ||                                 \
   __cpp_lib_uncaught_exceptions < 201411
namespace __cxxabiv1 {
struct __cxa_eh_globals;
extern "C" __cxa_eh_globals* __cxa_get_globals() noexcept
   __attribute__((__const__));
} // namespace __cxxabiv1

namespace std {
inline int uncaught_exceptions() noexcept {
	return *reinterpret_cast<unsigned*>(
	   reinterpret_cast<char*>(__cxxabiv1::__cxa_get_globals()) +
	   sizeof(void*));
}
} // namespace std
#endif

namespace stack_unwinding {

template <class Func>
class Guard {
	int uncaught_counter_ = std::uncaught_exceptions();
	Func func_;

	Guard(const Guard&) = delete;
	Guard(Guard&&) = delete;
	Guard& operator=(const Guard&) = delete;
	Guard& operator=(Guard&&) = delete;

public:
	Guard(Func func) noexcept : func_(std::move(func)) {}

	~Guard() {
		if (uncaught_counter_ != std::uncaught_exceptions()) {
			try {
				func_();
			} catch (...) {
			}
		}
	}
};

extern thread_local InplaceArray<InplaceBuff<1024>, 64> marks_collected;

#define STACK_UNWINDING_MARK                                                   \
	stack_unwinding::Guard CONCAT(stack_unwind_mark, __COUNTER__)(             \
	   [upper_func_name = __PRETTY_FUNCTION__] {                               \
		   auto& marks = stack_unwinding::marks_collected;                     \
		   marks.emplace_back(concat(                                          \
		      upper_func_name, " at " __FILE__ ":" STRINGIZE(__LINE__)));      \
	   })

} // namespace stack_unwinding
