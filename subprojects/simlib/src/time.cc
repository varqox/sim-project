#include <ctime>
#include <simlib/errmsg.hh>
#include <simlib/macros/throw.hh>
#include <simlib/string_view.hh>
#include <simlib/time.hh>
#include <string>
#include <sys/time.h>

namespace {

time_t current_time_t() {
    time_t curr_time;
    if (time(&curr_time) == static_cast<time_t>(-1)) {
        THROW("time()", errmsg());
    }
    return curr_time;
}

std::string mysql_datetime(
    struct tm* (*time_t_to_tm)(const time_t*, struct tm*),
    const char* time_t_to_tm_func_name,
    time_t time
) {
    struct tm t;
    if (!time_t_to_tm(&time, &t)) {
        THROW(time_t_to_tm_func_name, "()", errmsg());
    }
    std::string res(20, '\0');
    size_t len = strftime(res.data(), res.size(), "%Y-%m-%d %H:%M:%S", &t);
    res.resize(len);
    return res;
}

} // namespace

std::string utc_mysql_datetime_with_offset(time_t offset) {
    return mysql_datetime(gmtime_r, "gmtime_r", current_time_t() + offset);
}

std::string utc_mysql_datetime_from_time_t(time_t time) {
    return mysql_datetime(gmtime_r, "gmtime_r", time);
}

std::string local_mysql_datetime() {
    return mysql_datetime(localtime_r, "localtime_r", current_time_t());
}
