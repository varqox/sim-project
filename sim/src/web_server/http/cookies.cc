#include "cookies.hh"

#include <ctime>
#include <simlib/concat_tostr.hh>
#include <simlib/debug.hh>
#include <simlib/string_view.hh>

using std::string;

namespace web_server::http {

void Cookies::set(
    StringView name,
    StringView val,
    std::optional<time_t> expire,
    std::optional<StringView> path,
    bool http_only,
    bool secure
) {
    string value = concat_tostr(val, "; SameSite=Lax");
    if (expire) {
        std::array<char, 64> buff{{}};
        tm expire_tm = {};
        tm* rc1 = gmtime_r(&*expire, &expire_tm);
        assert(rc1 != nullptr);
        size_t rc2 = strftime(buff.data(), buff.size(), "%a, %d %b %Y %H:%M:%S GMT", &expire_tm);
        assert(rc2 > 0);
        back_insert(value, "; Expires=", buff.data());
    }
    if (path) {
        back_insert(value, "; Path=", *path);
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
