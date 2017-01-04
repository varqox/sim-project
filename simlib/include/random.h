#pragma once

#include <array>
#include <chrono>
#include <random>

// Fills @p bytes bytes of @p dest with random values
void fillRandomly(void* dest, size_t bytes) noexcept;

/**
 * @brief Reads @p bytes bytes from /dev/urandom to @p dest
 *
 * @param dest destination
 * @param bytes number of bytes to read
 *
 * @return @p bytes on success, -1 on error and errno set to one of
 *   possibilities from open(2) and read(2)
 */
ssize_t readFromDevUrandom_nothrow(void* dest, size_t bytes) noexcept;

/**
 * @brief Reads @p bytes bytes from /dev/urandom to @p dest
 *
 * @param dest destination
 * @param bytes number of bytes to read
 *
 * @errors If any error occurs an exception of type std::runtime_error is thrown
 */
void readFromDevUrandom(void* dest, size_t bytes);

inline std::mt19937 randomlySeededMersene() {
	std::array<std::mt19937::result_type, std::mt19937::state_size> seeds;
	readFromDevUrandom(&seeds[0], seeds.size() * sizeof(seeds[0]));
	std::seed_seq sseq(seeds.begin(), seeds.end());
	return std::mt19937 {sseq};
}

extern std::mt19937 random_generator;

// Get random from [a, b]
template<class T>
constexpr inline T getRandom(T&& a, T&& b) {
	return std::uniform_int_distribution<T>(std::forward<T>(a),
		std::forward<T>(b)) (random_generator);
}

template<class Iter>
void randomShuffle (Iter begin, Iter end) {
	for (auto n = end - begin, i = n - 1; i > 0; --i)
		std::swap(begin[i], begin[getRandom(0, i)]);
}
