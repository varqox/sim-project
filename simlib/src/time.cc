#include "../include/time.h"

#include <sys/time.h>
#include <algorithm>

long long microtime() {
	timeval mtime;
	gettimeofday(&mtime, NULL);
	return (mtime.tv_sec * 1000000LL) + mtime.tv_usec;
}

std::string date(const std::string& str, time_t cur_time) {
	if (cur_time == -1)
		time(&cur_time);
	size_t buff_size = str.size() + 1 + std::count(str.begin(), str.end(), '%') * 25;
	char buff[buff_size];
	tm *ptm = gmtime(&cur_time);
	strftime(buff, buff_size, str.c_str(), ptm);
	return buff;
}
