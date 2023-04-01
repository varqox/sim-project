#include "response.hh"

#include <simlib/time.hh>

namespace web_server::http {

void Response::set_cache(bool to_public, uint max_age, bool must_revalidate) {
    headers["expires"] = date("%a, %d %b %Y %H:%M:%S GMT", time(nullptr) + max_age);
    headers["cache-control"] = concat_tostr(
        (to_public ? "public" : "private"),
        (must_revalidate ? "; must-revalidate" : ""),
        "; max-age=",
        max_age
    );
}

} // namespace web_server::http
