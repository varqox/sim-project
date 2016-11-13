#include "../include/debug.h"

#include <algorithm>
#include <ctime>
#include <string>
#include <sys/time.h>

using std::string;

long long microtime() noexcept {
	timeval mtime;
	(void)gettimeofday(&mtime, nullptr);
	return (mtime.tv_sec * 1000000LL) + mtime.tv_usec;
}

template<class F>
static string __date(const string& str, time_t curr_time, F func) {
	if (curr_time < 0)
		time(&curr_time);

	size_t buff_size = str.size() + 1 +
		std::count(str.begin(), str.end(), '%') * 25;
	char buff[buff_size];
	tm *ptm = func(&curr_time);
	if (!ptm)
		THROW("Failed to convert time");

	if (strftime(buff, buff_size, str.c_str(), ptm))
		return buff;

	return {};
}

string date(const string& str, time_t curr_time) {
	return __date(str, curr_time, gmtime);
}

string localdate(const string& str, time_t curr_time) {
	return __date(str, curr_time, localtime);
}

bool isDatetime(const string& str) noexcept {
	struct tm t;
	return (str.size() == 19 &&
			strptime(str.c_str(), "%Y-%m-%d %H:%M:%S", &t) != nullptr);
}

time_t strToTime(const string& str, const char* format) noexcept {
	struct tm t;
	if (!strptime(str.c_str(), format, &t))
		return -1;

	return timegm(&t);
}
