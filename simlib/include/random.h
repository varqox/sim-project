#pragma once

#include <chrono>
#include <random>

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

inline uint_fast32_t getRandomSeed() noexcept {
	uint_fast32_t seed;
	return (-1 == readRandomBytes_nothrow(&seed, sizeof(seed)) ? std::chrono::system_clock::now().time_since_epoch().count() : seed);
}


extern std::mt19937 getRandom_generator;

// Get random from [a, b]
inline uint_fast32_t getRandom(uint_fast32_t a, uint_fast32_t b) {
	return std::uniform_int_distribution<uint_fast32_t>(a, b)
		(getRandom_generator);
}
