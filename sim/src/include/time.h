#pragma once

#include <ctime>
#include <string>

long long microtime();

std::string date(const std::string& str, time_t cur_time = -1);

// Checks if format is "%Y-%m-%d %H:%M:%S"
bool isDatetime(const std::string& str);

inline bool isDatetimeOrBlank(const std::string& str) {
	return str.empty() || isDatetime(str);
}