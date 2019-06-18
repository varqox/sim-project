#pragma once

#include "string.h"

#include <chrono>
#include <sys/time.h>

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
time_t strToTime(CStringView str, CStringView format = CStringView {
                                     "%Y-%m-%d %H:%M:%S"}) noexcept;

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

constexpr inline bool operator>(timeval a, timeval b) noexcept { return b < a; }

constexpr inline bool operator<=(timeval a, timeval b) noexcept {
	return (a.tv_sec < b.tv_sec or
	        (a.tv_sec == b.tv_sec and a.tv_usec <= b.tv_usec));
}

constexpr inline bool operator>=(timeval a, timeval b) noexcept {
	return b <= a;
}

template <size_t N = meta::ToString<UINT64_MAX>::arr_value.size() +
                     11> // +11
                         // for terminating null and decimal point and the
                         // fraction part
InplaceBuff<N> timespec_to_str(timespec x, uint prec,
                               bool trim_zeros = true) noexcept {
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

template <size_t N = meta::ToString<UINT64_MAX>::arr_value.size() +
                     8> // +8
                        // for terminating null and decimal point and the
                        // fraction part
InplaceBuff<N> timeval_to_str(timeval x, uint prec,
                              bool trim_zeros = true) noexcept {
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

constexpr bool is_power_of_10(intmax_t x) {
	if (x <= 0)
		return false;

	while (x > 1) {
		if (x % 10 != 0)
			return false;
		x /= 10;
	}

	return (x == 1);
}

/**
 * @brief Converts std::chrono::duration to string (as seconds)
 *
 * @param dur std::chrono::duration to convert to string
 * @param trim_zeros set whether to trim trailing zeros
 *
 * @return floating-point @p dur in seconds as string
 */
template <class Rep, class Period,
          size_t N =
             meta::ToString<std::numeric_limits<Rep>::max()>::arr_value.size() +
             3> // +3 for sign, terminating null and decimal point
InplaceBuff<N> toString(const std::chrono::duration<Rep, Period>& dur,
                        bool trim_zeros = true) noexcept {
	static_assert(Period::num == 1, "Needed below");
	static_assert(is_power_of_10(Period::den), "Needed below");
	auto dec_dur =
	   std::chrono::duration_cast<std::chrono::duration<intmax_t>>(dur);
	auto res = toString(dec_dur.count());
	res[res.size++] = '.';

	auto x = std::chrono::duration<intmax_t, Period>(dur - dec_dur).count();
	constexpr int prec = meta::ToString<Period::den>::arr_value.size() - 1;
	for (auto i = res.size + prec - 1; i >= res.size; --i) {
		res[i] = '0' + x % 10;
		x /= 10;
	}

	if (trim_zeros) {
		// Truncate trailing zeros
		auto i = res.size + prec - 1;
		// i will point to the last character of the result
		while (i >= res.size and res[i] == '0')
			--i;

		if (i == res.size - 1)
			res.size = i; // Trim trailing '.'
		else
			res.size = i + 1;
	} else {
		res.size += prec;
	}

	return res;
}

// TODO: This works for positive durations - check it for negative
template <class Rep, class Period>
timespec to_timespec(const std::chrono::duration<Rep, Period>& dur) noexcept {
	auto sec_dur = std::chrono::duration_cast<std::chrono::seconds>(dur);
	auto nsec =
	   std::chrono::duration_cast<std::chrono::nanoseconds>(dur - sec_dur);
	return {sec_dur.count(), nsec.count()};
}

inline std::chrono::nanoseconds to_nanoseconds(const timespec& ts) noexcept {
	return std::chrono::seconds(ts.tv_sec) +
	       std::chrono::nanoseconds(ts.tv_nsec);
}

template <class Rep, class Period>
inline auto
floor_to_10ms(const std::chrono::duration<Rep, Period>& time) noexcept {
	return std::chrono::duration_cast<
	   std::chrono::duration<intmax_t, std::centi>>(time);
}
