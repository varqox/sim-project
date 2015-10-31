#pragma once

#include "ncg.h"

#include <algorithm>

// Get random from [a, b]
inline int getRandom(int a, int b) {
	return a + pull() % (b - a + 1);
}

template<class Iter>
void randomShuffle (Iter begin, Iter end) {
	for (auto n = end - begin, i = n - 1; i > 0; --i)
		std::swap(begin[i], begin[getRandom(0, i)]);
}

// Fills @p bytes bytes of @p dest with random values
void fillRandomly(void* dest, size_t bytes) noexcept;

// Returns bytes read (from /dev/urandom) or -1 on error (errno from open(2)
// or read(2))
ssize_t readRandomBytes_nothrow(void* dest, size_t bytes) noexcept;

// Returns bytes read (from /dev/urandom) or -1 on error throws a
// std::runtime_error with appropriate message if error occurs
void readRandomBytes(void* dest, size_t bytes) noexcept(false);
