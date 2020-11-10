#pragma once

#include "simlib/debug.hh"
#include "simlib/string_view.hh"

#include <chrono>
#include <sys/time.h>
#include <utility>

int64_t microtime() noexcept;

// Returns UTC date in format of @p format (format like in strftime(3)), if
// @p curr_time >= 0 uses @p curr_time, otherwise uses the current time
std::string date(CStringView format, time_t curr_time = -1);

// Returns UTC date @p tp in format of @p format (format like in strftime(3))
inline std::string date(CStringView format, std::chrono::system_clock::time_point tp) {
    return date(format, std::chrono::system_clock::to_time_t(tp));
}

// Returns UTC date in format of "%Y-%m-%d %H:%M:%S" (see strftime(3)), if
// @p curr_time >= 0 uses @p curr_time, otherwise uses the current time
inline std::string mysql_date(time_t curr_time = -1) {
    return date("%Y-%m-%d %H:%M:%S", curr_time);
}

// Returns UTC date @p tp in format of "%Y-%m-%d %H:%M:%S" (see strftime(3))
inline std::string mysql_date(std::chrono::system_clock::time_point tp) {
    return mysql_date(std::chrono::system_clock::to_time_t(tp));
}

// Returns local date in format of @p format (format like in strftime(3)), if
// @p curr_time >= 0 uses @p curr_time, otherwise uses the current time
std::string localdate(CStringView format, time_t curr_time = -1);

// Returns local date @p tp in format of @p format (format like in strftime(3))
inline std::string localdate(CStringView format, std::chrono::system_clock::time_point tp) {
    return localdate(format, std::chrono::system_clock::to_time_t(tp));
}

// Returns local date in format of "%Y-%m-%d %H:%M:%S" (see strftime(3)), if
// @p curr_time >= 0 uses @p curr_time, otherwise uses the current time
inline std::string mysql_localdate(time_t curr_time = -1) {
    return localdate("%Y-%m-%d %H:%M:%S", curr_time);
}

// Returns local date @p tp in format of "%Y-%m-%d %H:%M:%S" (see strftime(3))
inline std::string mysql_localdate(std::chrono::system_clock::time_point tp) {
    return mysql_localdate(std::chrono::system_clock::to_time_t(tp));
}

// Checks if format is "%Y-%m-%d %H:%M:%S"
bool is_datetime(const CStringView& str) noexcept;

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
time_t
str_to_time_t(CStringView str, CStringView format = CStringView{"%Y-%m-%d %H:%M:%S"}) noexcept;

/**
 * @brief Converts a @p str containing time in format @p format to time_point
 * @errors The same that occur for str_to_time_t() but thrown as exceptions
 */
inline std::chrono::system_clock::time_point
str_to_time_point(CStringView str, CStringView format = CStringView{"%Y-%m-%d %H:%M:%S"}) {
    time_t t = str_to_time_t(str, format);
    if (t == -1) {
        THROW("str_to_time_t()", errmsg());
    }

    return std::chrono::system_clock::from_time_t(t);
}

/**************************** timespec arithmetic ****************************/
constexpr timespec operator+(timespec a, timespec b) noexcept {
    timespec res{a.tv_sec + b.tv_sec, a.tv_nsec + b.tv_nsec};
    if (res.tv_nsec >= 1'000'000'000) {
        ++res.tv_sec;
        res.tv_nsec -= 1'000'000'000;
    }

    return res;
}

constexpr timespec operator-(timespec a, timespec b) noexcept {
    timespec res{a.tv_sec - b.tv_sec, a.tv_nsec - b.tv_nsec};
    if (res.tv_nsec < 0) {
        --res.tv_sec;
        res.tv_nsec += 1'000'000'000;
    }

    return res;
}

constexpr timespec& operator+=(timespec& a, timespec b) noexcept { return (a = a + b); }

constexpr timespec& operator-=(timespec& a, timespec b) noexcept { return (a = a - b); }

constexpr bool operator==(timespec a, timespec b) noexcept {
    return (a.tv_sec == b.tv_sec and a.tv_nsec == b.tv_nsec);
}

constexpr bool operator!=(timespec a, timespec b) noexcept {
    return (a.tv_sec != b.tv_sec or a.tv_nsec != b.tv_nsec);
}

constexpr bool operator<(timespec a, timespec b) noexcept {
    return (a.tv_sec < b.tv_sec or (a.tv_sec == b.tv_sec and a.tv_nsec < b.tv_nsec));
}

constexpr bool operator>(timespec a, timespec b) noexcept { return b < a; }

constexpr bool operator<=(timespec a, timespec b) noexcept {
    return (a.tv_sec < b.tv_sec or (a.tv_sec == b.tv_sec and a.tv_nsec <= b.tv_nsec));
}

constexpr bool operator>=(timespec a, timespec b) noexcept { return b <= a; }

/**************************** timeval arithmetic ****************************/
constexpr timeval operator+(timeval a, timeval b) noexcept {
    timeval res{a.tv_sec + b.tv_sec, a.tv_usec + b.tv_usec};
    if (res.tv_usec >= 1'000'000) {
        ++res.tv_sec;
        res.tv_usec -= 1'000'000;
    }

    return res;
}

constexpr timeval operator-(timeval a, timeval b) noexcept {
    timeval res{a.tv_sec - b.tv_sec, a.tv_usec - b.tv_usec};
    if (res.tv_usec < 0) {
        --res.tv_sec;
        res.tv_usec += 1'000'000;
    }

    return res;
}

constexpr timeval& operator+=(timeval& a, timeval b) noexcept { return (a = a + b); }

constexpr timeval& operator-=(timeval& a, timeval b) noexcept { return (a = a - b); }
constexpr bool operator==(timeval a, timeval b) noexcept {
    return (a.tv_sec == b.tv_sec and a.tv_usec == b.tv_usec);
}

constexpr bool operator!=(timeval a, timeval b) noexcept {
    return (a.tv_sec != b.tv_sec or a.tv_usec != b.tv_usec);
}

constexpr bool operator<(timeval a, timeval b) noexcept {
    return (a.tv_sec < b.tv_sec or (a.tv_sec == b.tv_sec and a.tv_usec < b.tv_usec));
}

constexpr bool operator>(timeval a, timeval b) noexcept { return b < a; }

constexpr bool operator<=(timeval a, timeval b) noexcept {
    return (a.tv_sec < b.tv_sec or (a.tv_sec == b.tv_sec and a.tv_usec <= b.tv_usec));
}

constexpr bool operator>=(timeval a, timeval b) noexcept { return b <= a; }

template <
    size_t N = decltype(to_string(std::declval<uintmax_t>()))::max_size() +
        11> // +11
            // for terminating null and decimal point and the
            // fraction part
constexpr StaticCStringBuff<N>
timespec_to_string(timespec x, uint prec, bool trim_zeros = true) noexcept {
    auto res = to_string<uint64_t, N>(x.tv_sec);
    res[res.len_++] = '.';
    for (unsigned i = res.len_ + 8; i >= res.len_; --i) {
        res[i] = '0' + x.tv_nsec % 10;
        x.tv_nsec /= 10;
    }

    // Truncate trailing zeros
    unsigned i = res.len_ + std::min(8, int(prec) - 1);
    // i will point to the last character of the result
    if (trim_zeros) {
        while (i >= res.len_ && res[i] == '0') {
            --i;
        }
    }

    if (i == res.len_ - 1) {
        res.len_ = i; // Trim trailing '.'
    } else {
        res.len_ = ++i;
    }

    *res.end() = '\0';
    return res;
}

template <
    size_t N = decltype(to_string(std::declval<uintmax_t>()))::max_size() +
        8> // +8
           // for terminating null and decimal point and the
           // fraction part
constexpr StaticCStringBuff<N>
timeval_to_string(timeval x, uint prec, bool trim_zeros = true) noexcept {
    auto res = to_string<uint64_t, N>(x.tv_sec);
    res[res.len_++] = '.';
    for (unsigned i = res.len_ + 5; i >= res.len_; --i) {
        res[i] = '0' + x.tv_usec % 10;
        x.tv_usec /= 10;
    }

    // Truncate trailing zeros
    unsigned i = res.len_ + std::min(5, int(prec) - 1);
    // i will point to the last character of the result
    if (trim_zeros) {
        while (i >= res.len_ && res[i] == '0') {
            --i;
        }
    }

    if (i == res.len_ - 1) {
        res.len_ = i; // Trim trailing '.'
    } else {
        res.len_ = ++i;
    }

    *res.end() = '\0';
    return res;
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr bool is_power_of_10(T x) noexcept {
    if (x <= 0) {
        return false;
    }

    while (x > 1) {
        if (x % 10 != 0) {
            return false;
        }
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
template <
    class Rep, class Period,
    size_t N = decltype(to_string(std::declval<Rep>()))::max_size() +
        3> // +3 for sign, terminating null and decimal point
constexpr StaticCStringBuff<N>
to_string(const std::chrono::duration<Rep, Period>& dur, bool trim_zeros = true) noexcept {
    static_assert(Period::num == 1, "Needed below");
    static_assert(is_power_of_10(Period::den), "Needed below");
    auto dec_dur = std::chrono::duration_cast<std::chrono::duration<intmax_t>>(dur);
    auto res = to_string<Rep, N>(dec_dur.count());
    res[res.len_++] = '.';

    auto x = std::chrono::duration<intmax_t, Period>(dur - dec_dur).count();
    constexpr int prec = to_string(Period::den).size() - 1;
    for (auto i = res.len_ + prec - 1; i >= res.len_; --i) {
        res[i] = '0' + x % 10;
        x /= 10;
    }

    if (trim_zeros) {
        // Truncate trailing zeros
        auto i = res.len_ + prec - 1;
        // i will point to the last character of the result
        while (i >= res.len_ and res[i] == '0') {
            --i;
        }

        if (i == res.len_ - 1) {
            res.len_ = i; // Trim trailing '.'
        } else {
            res.len_ = i + 1;
        }
    } else {
        res.len_ += prec;
    }

    *res.end() = '\0';
    return res;
}

constexpr std::chrono::microseconds to_duration(const timeval& tv) noexcept {
    return std::chrono::seconds(tv.tv_sec) + std::chrono::microseconds(tv.tv_usec);
}

constexpr std::chrono::nanoseconds to_duration(const timespec& ts) noexcept {
    return std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec);
}

// TODO: This works for positive durations - check it for negative
template <class Rep, class Period>
timespec to_timespec(const std::chrono::duration<Rep, Period>& dur) noexcept {
    auto sec_dur = std::chrono::duration_cast<std::chrono::seconds>(dur);
    auto nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(dur - sec_dur);
    return {sec_dur.count(), nsec.count()};
}

constexpr std::chrono::nanoseconds to_nanoseconds(const timespec& ts) noexcept {
    return std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec);
}

template <class Rep, class Period>
constexpr auto floor_to_10ms(const std::chrono::duration<Rep, Period>& time) noexcept {
    return std::chrono::duration_cast<std::chrono::duration<intmax_t, std::centi>>(time);
}
