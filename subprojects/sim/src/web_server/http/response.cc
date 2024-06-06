#include "response.hh"

#include <simlib/concat_tostr.hh>
#include <simlib/errmsg.hh>
#include <simlib/macros/throw.hh>
#include <simlib/time.hh>

namespace web_server::http {

void Response::set_cache(bool to_public, uint max_age_in_seconds, bool must_revalidate) {
    time_t curr_time;
    if (time(&curr_time) == static_cast<time_t>(-1)) {
        THROW("time()", errmsg());
    }
    curr_time += max_age_in_seconds;
    struct tm t;
    if (!gmtime_r(&curr_time, &t)) {
        THROW("gmtime_r()", errmsg());
    }
    std::string datetime(64, '\0');
    size_t len = strftime(datetime.data(), datetime.size(), "%a, %d %b %Y %H:%M:%S GMT", &t);
    datetime.resize(len);

    headers["expires"] = std::move(datetime);
    headers["cache-control"] = concat_tostr(
        (to_public ? "public" : "private"),
        (must_revalidate ? "; must-revalidate" : ""),
        "; max-age=",
        max_age_in_seconds
    );
}

} // namespace web_server::http
