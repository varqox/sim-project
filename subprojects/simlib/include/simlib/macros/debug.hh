#pragma once

#ifdef DEBUG
#define D(...) __VA_ARGS__
#define ND(...)
#else
#define D(...)
#define ND(...) __VA_ARGS__
#endif
