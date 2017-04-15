#pragma once

#include "string.h"

long long microtime() noexcept;

// Returns UTC date in format of @p format (format like in strftime(3)), if
// @p curr_time > 0 uses @p curr_time, otherwise uses the current time
std::string date(CStringView format = CStringView {"%Y-%m-%d %H:%M:%S"},
	time_t curr_time = -1);

// Returns local date in format of @p format (format like in strftime(3)), if
// @p curr_time > 0 uses @p curr_time, otherwise uses the current time
std::string localdate(CStringView format = CStringView {"%Y-%m-%d %H:%M:%S"},
	time_t curr_time = -1);

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
