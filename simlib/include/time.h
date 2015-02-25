#pragma once

#include <ctime>
#include <string>

long long microtime();
std::string date(const std::string& str, time_t cur_time = -1);
