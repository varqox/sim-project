#pragma once

#include <string>

long long microtime();

// Returns UTC date in format of @p str (format like in strftime(3)), if
// @p curr_time > 0 uses @p curr_time, otherwise uses the current time
std::string date(const std::string& str, time_t curr_time = -1);

// Returns local date in format of @p str (format like in strftime(3)), if
// @p curr_time > 0 uses @p curr_time, otherwise uses the current time
std::string localdate(const std::string& str, time_t curr_time = -1);

// Checks if format is "%Y-%m-%d %H:%M:%S"
bool isDatetime(const std::string& str);
