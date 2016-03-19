#pragma once

#include <stdio.h>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

#ifdef DEBUG
# define D(...) __VA_ARGS__
#else
# define D(...)
#endif

#define E(...) eprintf(__VA_ARGS__)

// Very useful - includes exception origin
#define THROW(...) throw std::runtime_error(concat(__VA_ARGS__, " (thrown at " \
	__FILE__ ":", toString(__LINE__), ')'))

#define ERRLOG_CATCH() errlog(__FILE__ ":", toString(__LINE__), \
	": Caught exception")

#define ERRLOG_CAUGHT(e) errlog(__FILE__ ":", toString(__LINE__), \
	": Caught exception -> ", e.what())
