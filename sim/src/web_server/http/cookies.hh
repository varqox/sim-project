#pragma once

#include "simlib/string_view.hh"
#include "src/web_server/http/headers.hh"

namespace web_server::http {

struct Cookies {
    Headers cookies_as_headers;

    void
    set(StringView name, const std::string& val, time_t expire = -1,
        const std::string& path = "", const std::string& domain = "", bool http_only = false,
        bool secure = false);

    StringView get(StringView name) const noexcept {
        StringView cookie = cookies_as_headers.get(name);
        return cookie.substr(0, cookie.find(';'));
    }
};

} // namespace web_server::http
