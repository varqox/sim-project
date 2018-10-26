#pragma once

#include "string.h"

#include <ctime>

long long microtime() noexcept;

// Returns UTC date in format of @p format (format like in strftime(3)), if
// @p curr_time >= 0 uses @p curr_time, otherwise uses the current time
std::string date(CStringView format, time_t curr_time = -1);

// Returns UTC date in format of "%Y-%m-%d %H:%M:%S" (see strftime(3)), if
// @p curr_time >= 0 uses @p curr_time, otherwise uses the current time
inline std::string mysql_date(time_t curr_time = -1) {
	return date("%Y-%m-%d %H:%M:%S", curr_time);
}

// Returns local date in format of @p format (format like in strftime(3)), if
// @p curr_time >= 0 uses @p curr_time, otherwise uses the current time
std::string localdate(CStringView format, time_t curr_time = -1);

// Returns local date in format of "%Y-%m-%d %H:%M:%S" (see strftime(3)), if
// @p curr_time >= 0 uses @p curr_time, otherwise uses the current time
inline std::string mysql_localdate(time_t curr_time = -1) {
	return localdate("%Y-%m-%d %H:%M:%S", curr_time);
}

// Checks if format is "%Y-%m-%d %H:%M:%S"
bool isDatetime(CStringView str) noexcept;

/**
 * @brief Converts a string containing time to time_t
 *
 * @param str a string containing time
 * @param format the format of the contained time
 *
 * @return time as time_t, or -1 if an error occurred
 *
 * @errors The same that occur for strptime(3) and timegm(3)
 */
time_t strToTime(CStringView str,
	CStringView format = CStringView {"%Y-%m-%d %H:%M:%S"}) noexcept;

/**************************** timespec arithmetic ****************************/
constexpr inline timespec operator+(timespec a, timespec b) noexcept {
	timespec res {a.tv_sec + b.tv_sec, a.tv_nsec + b.tv_nsec};
	if (res.tv_nsec >= 1'000'000'000) {
		++res.tv_sec;
		res.tv_nsec -= 1'000'000'000;
	}

	return res;
}

constexpr inline timespec operator-(timespec a, timespec b) noexcept {
	timespec res {a.tv_sec - b.tv_sec, a.tv_nsec - b.tv_nsec};
	if (res.tv_nsec < 0) {
		--res.tv_sec;
		res.tv_nsec += 1'000'000'000;
	}

	return res;
}

constexpr inline timespec& operator+=(timespec& a, timespec b) noexcept {
	return (a = a + b);
}

constexpr inline timespec& operator-=(timespec& a, timespec b) noexcept {
	return (a = a - b);
}

constexpr inline bool operator==(timespec a, timespec b) noexcept {
	return (a.tv_sec == b.tv_sec and a.tv_nsec == b.tv_nsec);
}

constexpr inline bool operator!=(timespec a, timespec b) noexcept {
	return (a.tv_sec != b.tv_sec or a.tv_nsec != b.tv_nsec);
}

constexpr inline bool operator<(timespec a, timespec b) noexcept {
	return (a.tv_sec < b.tv_sec or
		(a.tv_sec == b.tv_sec and a.tv_nsec < b.tv_nsec));
}

constexpr inline bool operator>(timespec a, timespec b) noexcept {
	return b < a;
}

constexpr inline bool operator<=(timespec a, timespec b) noexcept {
	return (a.tv_sec < b.tv_sec or
		(a.tv_sec == b.tv_sec and a.tv_nsec <= b.tv_nsec));
}

constexpr inline bool operator>=(timespec a, timespec b) noexcept {
	return b <= a;
}

/**************************** timeval arithmetic ****************************/
constexpr inline timeval operator+(timeval a, timeval b) noexcept {
	timeval res {a.tv_sec + b.tv_sec, a.tv_usec + b.tv_usec};
	if (res.tv_usec >= 1'000'000) {
		++res.tv_sec;
		res.tv_usec -= 1'000'000;
	}

	return res;
}

constexpr inline timeval operator-(timeval a, timeval b) noexcept {
	timeval res {a.tv_sec - b.tv_sec, a.tv_usec - b.tv_usec};
	if (res.tv_usec < 0) {
		--res.tv_sec;
		res.tv_usec += 1'000'000;
	}

	return res;
}

constexpr inline timeval& operator+=(timeval& a, timeval b) noexcept {
	return (a = a + b);
}

constexpr inline timeval& operator-=(timeval& a, timeval b) noexcept {
	return (a = a - b);
}
constexpr inline bool operator==(timeval a, timeval b) noexcept {
	return (a.tv_sec == b.tv_sec and a.tv_usec == b.tv_usec);
}

constexpr inline bool operator!=(timeval a, timeval b) noexcept {
	return (a.tv_sec != b.tv_sec or a.tv_usec != b.tv_usec);
}

constexpr inline bool operator<(timeval a, timeval b) noexcept {
	return (a.tv_sec < b.tv_sec or
		(a.tv_sec == b.tv_sec and a.tv_usec < b.tv_usec));
}

constexpr inline bool operator>(timeval a, timeval b) noexcept {
	return b < a;
}

constexpr inline bool operator<=(timeval a, timeval b) noexcept {
	return (a.tv_sec < b.tv_sec or
		(a.tv_sec == b.tv_sec and a.tv_usec <= b.tv_usec));
}

constexpr inline bool operator>=(timeval a, timeval b) noexcept {
	return b <= a;
}

template<size_t N = meta::ToString<UINT64_MAX>::arr_value.size() + 11> // +11
    // for terminating null and decimal point and the fraction part
InplaceBuff<N> timespec_to_str(timespec x, uint prec, bool trim_zeros = true) {
	auto res = toString<uint64_t, N>(x.tv_sec);
	res[res.size++] = '.';
	for (unsigned i = res.size + 8; i >= res.size; --i) {
		res[i] = '0' + x.tv_nsec % 10;
		x.tv_nsec /= 10;
	}

	// Truncate trailing zeros
	unsigned i = res.size + std::min(8, int(prec) - 1);
	// i will point to the last character of the result
	if (trim_zeros)
		while (i >= res.size && res[i] == '0')
			--i;

	if (i == res.size - 1)
		res.size = i; // Trim trailing '.'
	else
		res.size = ++i;

	return res;
}

template<size_t N = meta::ToString<UINT64_MAX>::arr_value.size() + 8> // +8
    // for terminating null and decimal point and the fraction part
InplaceBuff<N> timeval_to_str(timeval x, uint prec, bool trim_zeros = true) {
	auto res = toString<uint64_t, N>(x.tv_sec);
	res[res.size++] = '.';
	for (unsigned i = res.size + 5; i >= res.size; --i) {
		res[i] = '0' + x.tv_usec % 10;
		x.tv_usec /= 10;
	}

	// Truncate trailing zeros
	unsigned i = res.size + std::min(5, int(prec) - 1);
	// i will point to the last character of the result
	if (trim_zeros)
		while (i >= res.size && res[i] == '0')
			--i;

	if (i == res.size - 1)
		res.size = i; // Trim trailing '.'
	else
		res.size = ++i;

	return res;
}


/**
 * @brief Converts usec (ULL) to sec (double as string)
 *
 * @param x usec value
 * @param prec precision (maximum number of digits after '.', if greater than 6,
 *   then it is downgraded to 6)
 * @param trim_zeros set whether to trim trailing zeros
 *
 * @return floating-point x in seconds as string
 */
template<size_t N = meta::ToString<UINT64_MAX>::arr_value.size() + 2> // +2 for
	// terminating null and decimal point
InplaceBuff<N> usecToSecStr(uint64_t x, uint prec, bool trim_zeros = true) {
	uint64_t y = x / 1'000'000;
	x -= y * 1'000'000;
	return timeval_to_str<N>(
		{static_cast<decltype(timeval::tv_sec)>(y),
			static_cast<decltype(timeval::tv_usec)>(x)},
		prec, trim_zeros);
}
