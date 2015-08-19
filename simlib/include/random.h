#pragma once

// Get random from [a, b]
inline int getRandom(int a, int b) {
	return a + rand() % (b - a + 1);
}
