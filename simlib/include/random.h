#pragma once

#include "ncg.h"

#include <algorithm>

// Get random from [a, b]
inline int getRandom(int a, int b) {
	return a + pull() % (b - a + 1);
}

template<class Iter>
void randomShuffle (Iter begin, Iter end) {
	for (__typeof(end - begin) n = end - begin, i = n - 1; i > 0; --i)
		std::swap(begin[i], begin[getRandom(0, i)]);
}
