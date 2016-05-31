#pragma once

#include <ctime>
#include <string>

long long microtime();

// Returns UTC date in format of @p str (format like in strftime(3)), if
// curr_time > 0 uses cur_time, otherwise uses the current time
std::string date(const std::string& str, time_t cur_time = -1);

// Returns local date in format of @p str (format like in strftime(3)), if
// curr_time > 0 uses cur_time, otherwise uses the current time
std::string localdate(const std::string& str, time_t cur_time = -1);

// Checks if format is "%Y-%m-%d %H:%M:%S"
bool isDatetime(const std::string& str);
