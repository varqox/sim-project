#pragma once

#include <cstdio>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

#ifdef DEBUG

# define D(...) __VA_ARGS__
# define E(...) eprintf(__VA_ARGS__)

#else

# define D(...)
# define E(...)

#endif
