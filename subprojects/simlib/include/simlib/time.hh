#pragma once

#include <ctime>
#include <string>

// Returns current UTC + offset time in format of "%Y-%m-%d %H:%M:%S" (see strftime(3))
std::string utc_mysql_datetime(time_t offset = 0);

// Returns current local time in format of "%Y-%m-%d %H:%M:%S" (see strftime(3))
std::string local_mysql_datetime();

// Checks if format is "%Y-%m-%d %H:%M:%S"
inline bool is_datetime(const char* str) noexcept {
    struct tm t = {};
    return (
        std::char_traits<char>::length(str) == 19 &&
        strptime(str, "%Y-%m-%d %H:%M:%S", &t) == str + 19
    );
}
