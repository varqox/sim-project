#include <ctime>
#include <simlib/errmsg.hh>
#include <simlib/macros/throw.hh>
#include <simlib/string_view.hh>
#include <string>
#include <sys/time.h>

namespace {

std::string mysql_datetime(
    struct tm* (*time_t_to_tm)(const time_t*, struct tm*),
    const char* time_t_to_tm_func_name,
    time_t offset
) {
    time_t curr_time;
    if (time(&curr_time) == static_cast<time_t>(-1)) {
        THROW("time()", errmsg());
    }
    curr_time += offset;
    struct tm t;
    if (!time_t_to_tm(&curr_time, &t)) {
        THROW(time_t_to_tm_func_name, "()", errmsg());
    }
    std::string res(20, '\0');
    size_t len = strftime(res.data(), res.size(), "%Y-%m-%d %H:%M:%S", &t);
    res.resize(len);
    return res;
}

} // namespace

std::string utc_mysql_datetime(time_t offset) {
    return mysql_datetime(gmtime_r, "gmtime_r", offset);
}

std::string local_mysql_datetime() { return mysql_datetime(localtime_r, "localtime_r", 0); }
