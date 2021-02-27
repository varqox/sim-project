#include "src/web_server/http/cookies.hh"
#include "simlib/debug.hh"

#include <ctime>

using std::string;

namespace web_server::http {

void Cookies::set(
    StringView name, const string& val, time_t expire, const string& path,
    const string& domain, bool http_only, bool secure) {
    STACK_UNWINDING_MARK;

    string value = val;

    if (expire != -1) {
        char buff[35];
        tm* ptm = gmtime(&expire);
        if (strftime(buff, 35, "%a, %d %b %Y %H:%M:%S GMT", ptm)) {
            value.append("; Expires=").append(buff);
        }
    }

    if (!path.empty()) {
        back_insert(value, "; Path=", path);
    }

    if (!domain.empty()) {
        back_insert(value, "; Domain=", domain);
    }

    if (http_only) {
        value.append("; HttpOnly");
    }

    if (secure) {
        value.append("; Secure");
    }

    cookies_as_headers[name] = value;
}

} // namespace web_server::http
