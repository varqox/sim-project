#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <random>

// Fills @p bytes bytes of @p dest with random values
void fill_randomly(void* dest, size_t bytes);

/**
 * @brief Reads @p bytes bytes from /dev/urandom to @p dest
 *
 * @param dest destination
 * @param bytes number of bytes to read
 *
 * @return @p bytes on success, -1 on error and errno set to one of
 *   possibilities from open(2) and read(2)
 */
ssize_t read_from_dev_urandom_nothrow(void* dest, size_t bytes) noexcept;

/**
 * @brief Reads @p bytes bytes from /dev/urandom to @p dest
 *
 * @param dest destination
 * @param bytes number of bytes to read
 *
 * @errors If any error occurs an exception of type std::runtime_error is
 *   thrown
 */
void read_from_dev_urandom(void* dest, size_t bytes);

class RandomDevice {
public:
	using result_type = uint64_t;

	constexpr static result_type min() noexcept {
		return std::numeric_limits<result_type>::min();
	}

	constexpr static result_type max() noexcept {
		return std::numeric_limits<result_type>::max();
	}

private:
	std::array<result_type, 256 / sizeof(result_type)> buff{};
	size_t pos = buff.size();

	void fill_buff() {
		fill_randomly(buff.data(), buff.size() * sizeof(result_type));
		pos = 0;
	}

public:
	RandomDevice() { fill_buff(); }

	RandomDevice(const RandomDevice&) = delete;
	RandomDevice(RandomDevice&&) noexcept = default;
	RandomDevice& operator=(const RandomDevice&) = delete;
	RandomDevice& operator=(RandomDevice&&) noexcept = default;

	~RandomDevice() = default;

	result_type operator()() {
		if (pos == buff.size()) {
			fill_buff();
		}
		return buff[pos++];
	}
};

inline RandomDevice& get_random_generator() {
	static thread_local RandomDevice random_generator;
	return random_generator;
}

// Get random from [a, b]
template <class T>
constexpr T get_random(T&& a, T&& b) {
	return std::uniform_int_distribution<T>(
	   std::forward<T>(a), std::forward<T>(b))(get_random_generator());
}

template <class Iter>
void random_shuffle(Iter begin, Iter end) {
	for (auto n = end - begin, i = n - 1; i > 0; --i) {
		std::swap(begin[i], begin[get_random(0, i)]);
	}
}
