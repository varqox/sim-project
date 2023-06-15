#pragma once

#include <chrono>
#include <simlib/string_view.hh>
#include <sys/time.h>

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
