#pragma once

#include <ctime>
#include <string>
#include <sys/time.h>

long long microtime();

std::string date(const std::string& str, time_t cur_time = -1);

// Checks if format is "%Y-%m-%d %H:%M:%S"
bool isDatetime(const std::string& str);

class Stopwatch {
	struct timeval begin;

public:
	Stopwatch() noexcept { restart(); }

	void restart() noexcept { gettimeofday(&begin, nullptr); }

	long long microtime() const noexcept {
		struct timeval end;
		gettimeofday(&end, NULL);
		return (end.tv_sec - begin.tv_sec) * 1000000LL + end.tv_usec -
			begin.tv_usec;
	}

	double time() const noexcept { return microtime() * 0.000001; }
};
