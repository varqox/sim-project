#pragma once

#include "logger.h"
#include "string.h"

#include <cstdio>
#include <exception>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

#define try_assert(expr) \
	((expr) ? (void)0 : throw std::runtime_error(concat(__FILE__, ':', \
	toString(__LINE__), ": ", __PRETTY_FUNCTION__, \
	": Assertion `" #expr " failed.")))

#ifdef DEBUG
# define D(...) __VA_ARGS__
#else
# define D(...)
#endif

#define E(...) eprintf(__VA_ARGS__)

// Very useful - includes exception origin
#define THROW(...) throw std::runtime_error(concat(__VA_ARGS__, " (thrown at " \
	__FILE__ ":", toString(__LINE__), ')'))

namespace __debug {

constexpr inline bool is_VA_empty() { return true; }

template<class T1, class... T>
constexpr inline bool is_VA_empty(T1&&, T&&...) { return false; }

constexpr inline const char* __what() { return ""; }

inline const char* __what(const std::exception& e) {
	return e.what();
}

} // __debug

#define ERRLOG_CATCH(...) errlog(__FILE__ ":", toString(__LINE__), \
	": Caught exception", __debug::is_VA_empty(__VA_ARGS__) ? "" : " -> ", \
	__debug::__what(__VA_ARGS__))

#define ERRLOG_AND_FORWARD(...) { errlog(__FILE__ ":", \
		toString(__LINE__), ": Forwarding exception...", \
		__debug::is_VA_empty(__VA_ARGS__) ? "" : " -> ", \
		__debug::__what(__VA_ARGS__)); \
	throw; }
