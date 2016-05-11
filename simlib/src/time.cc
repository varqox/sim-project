#include <algorithm>
#include <sys/time.h>

using std::string;

long long microtime() {
	timeval mtime;
	gettimeofday(&mtime, nullptr);
	return (mtime.tv_sec * 1000000LL) + mtime.tv_usec;
}

string date(const string& str, time_t cur_time) {
	if (cur_time < 0)
		time(&cur_time);

	size_t buff_size = str.size() + 1 +
		std::count(str.begin(), str.end(), '%') * 25;
	char buff[buff_size];
	tm *ptm = gmtime(&cur_time);
	strftime(buff, buff_size, str.c_str(), ptm);

	return buff;
}

bool isDatetime(const string& str) {
	struct tm t;
	return (str.size() == 19 &&
			strptime(str.c_str(), "%Y-%m-%d %H:%M:%S", &t) != nullptr);
}
