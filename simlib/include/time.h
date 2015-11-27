#pragma once

#include <chrono>
#include <ctime>
#include <string>

long long microtime();

std::string date(const std::string& str, time_t cur_time = -1);

// Checks if format is "%Y-%m-%d %H:%M:%S"
bool isDatetime(const std::string& str);

class Stopwatch {
	std::chrono::steady_clock::time_point begin;

public:
	Stopwatch() noexcept { restart(); }

	void restart() noexcept { begin = std::chrono::steady_clock::now(); }

	long long microtime() const noexcept {
		using namespace std::chrono;
		return duration_cast<microseconds>(steady_clock::now() - begin).count();
	}

	double time() const noexcept {
		using namespace std::chrono;
		return duration<double>(steady_clock::now() - begin).count();
	}
};
